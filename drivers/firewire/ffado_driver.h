/*
 *   FireWire Backend for Jack
 *   using FFADO
 *   FFADO = Firewire (pro-)audio for linux
 *
 *   http://www.ffado.org
 *   http://www.jackaudio.org
 *
 *   Copyright (C) 2005-2007 Pieter Palmers
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* 
 * Main Jack driver entry routines
 *
 */ 
 
#ifndef __JACK_FFADO_DRIVER_H__
#define __JACK_FFADO_DRIVER_H__

#define FFADO_DRIVER_WITH_ASEQ_MIDI

#include <libffado/ffado.h>

#include <jack/driver.h>
#include <jack/engine.h>
#include <jack/types.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <endian.h>

#include <pthread.h>
#include <semaphore.h>

#include <jack/internal.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>
#include <jack/driver.h>
#include <jack/engine.h>
#include <jack/types.h>
#include <jack/thread.h>

#include <alsa/asoundlib.h>

// debug print control flags
#define DEBUG_LEVEL_BUFFERS           	(1<<0)
#define DEBUG_LEVEL_HANDLERS			(1<<1)
#define DEBUG_LEVEL_XRUN_RECOVERY     	(1<<2)
#define DEBUG_LEVEL_WAIT     			(1<<3)

#define DEBUG_LEVEL_RUN_CYCLE         	(1<<8)

#define DEBUG_LEVEL_PACKETCOUNTER		(1<<16)
#define DEBUG_LEVEL_STARTUP				(1<<17)
#define DEBUG_LEVEL_THREADS				(1<<18)


#ifdef DEBUG_ENABLED

	// default debug level
	#define DEBUG_LEVEL (  DEBUG_LEVEL_RUN_CYCLE | \
	(DEBUG_LEVEL_XRUN_RECOVERY)| DEBUG_LEVEL_STARTUP | DEBUG_LEVEL_WAIT | DEBUG_LEVEL_PACKETCOUNTER)

	#warning Building debug build!

	#define printMessage(format, args...) jack_error( "firewire MSG: %s:%d (%s): " format,  __FILE__, __LINE__, __FUNCTION__, ##args )
	#define printError(format, args...) jack_error( "firewire ERR: %s:%d (%s): " format,  __FILE__, __LINE__, __FUNCTION__, ##args )
	
	//#define printEnter() jack_error( "FBDRV ENTERS: %s (%s)\n", __FUNCTION__,  __FILE__)
	//#define printExit() jack_error( "FBDRV EXITS: %s (%s)\n", __FUNCTION__,  __FILE__)
	#define printEnter() 
	#define printExit() 
	
	#define debugError(format, args...) jack_error( "firewire ERR: %s:%d (%s): " format,  __FILE__, __LINE__, __FUNCTION__, ##args )
	#define debugPrint(Level, format, args...) if(DEBUG_LEVEL & (Level))  jack_error("DEBUG %s:%d (%s) :"  format, __FILE__, __LINE__, __FUNCTION__, ##args );
	#define debugPrintShort(Level, format, args...) if(DEBUG_LEVEL & (Level))  jack_error( format,##args );
	#define debugPrintWithTimeStamp(Level, format, args...) if(DEBUG_LEVEL & (Level)) jack_error( "%16lu: "format, debugGetCurrentUTime(),##args );
	#define SEGFAULT int *test=NULL;	*test=1;
#else
	#define DEBUG_LEVEL
	
	#define printMessage(format, args...) if(g_verbose) \
	                                         jack_error("firewire MSG: " format, ##args )
	#define printError(format, args...)   jack_error("firewire ERR: " format, ##args )
	
	#define printEnter() 
	#define printExit() 
	
	#define debugError(format, args...) 
	#define debugPrint(Level, format, args...) 
	#define debugPrintShort(Level, format, args...)	
	#define debugPrintWithTimeStamp(Level, format, args...)
#endif

// thread priority setup
#define FFADO_RT_PRIORITY_PACKETIZER_RELATIVE	5

#ifdef FFADO_DRIVER_WITH_ASEQ_MIDI

	#define ALSA_SEQ_BUFF_SIZE 1024
	#define MIDI_TRANSMIT_BUFFER_SIZE 1024
	#define MIDI_THREAD_SLEEP_TIME_USECS 100
	// midi priority should be higher than the audio priority in order to
	// make sure events are not only delivered on period boundarys
	// but I think it should be smaller than the packetizer thread in order not 
	// to lose any packets
	#define FFADO_RT_PRIORITY_MIDI_RELATIVE 	4

#endif

typedef struct _ffado_driver ffado_driver_t;

/*
 * Jack Driver command line parameters
 */

typedef struct _ffado_jack_settings ffado_jack_settings_t;
struct _ffado_jack_settings {
    int verbose_level;

    int period_size_set;
    jack_nframes_t period_size;
    
    int sample_rate_set;
    int sample_rate;
    
    int buffer_size_set;
    jack_nframes_t buffer_size;

    int playback_ports;
    int capture_ports;
    
    jack_nframes_t capture_frame_latency;
    jack_nframes_t playback_frame_latency;
    
    int slave_mode;
    int snoop_mode;
    
    char *device_info;
};

#ifdef FFADO_DRIVER_WITH_ASEQ_MIDI

typedef struct {
	int stream_nr;
	int seq_port_nr;
	snd_midi_event_t *parser;
	snd_seq_t *seq_handle;
} ffado_midi_port_t;

typedef struct _ffado_driver_midi_handle {
	ffado_device_t *dev;
	ffado_driver_t *driver;

	snd_seq_t *seq_handle;
	
	pthread_t queue_thread;
	pthread_t dequeue_thread;
	int queue_thread_realtime;
	int queue_thread_priority;

	int nb_input_ports;
	int nb_output_ports;

	ffado_midi_port_t **input_ports;
	ffado_midi_port_t **output_ports;

	ffado_midi_port_t **input_stream_port_map;
	int *output_port_stream_map;


} ffado_driver_midi_handle_t;

#endif
/*
 * JACK driver structure
 */
 

struct _ffado_driver
{
	JACK_DRIVER_NT_DECL;
	
	jack_nframes_t  sample_rate;
	jack_nframes_t  period_size;
	unsigned long   wait_time;

    jack_time_t                   wait_last;
    jack_time_t                   wait_next;
	int wait_late;
	
	jack_client_t  *client;
	
	int		xrun_detected;
	int		xrun_count;
	
	int process_count;
	
	/* settings from the command line */
	ffado_jack_settings_t settings;
	
	/* the firewire virtual device */
	ffado_device_t *dev;
	
    JSList                       *capture_ports;
    JSList                       *playback_ports;
    JSList                       *monitor_ports;
    channel_t                     playback_nchannels;
    channel_t                     capture_nchannels;

	jack_nframes_t  playback_frame_latency;
	jack_nframes_t  capture_frame_latency;
    	
	ffado_device_info_t device_info;
	ffado_options_t device_options;

#ifdef FFADO_DRIVER_WITH_ASEQ_MIDI
	ffado_driver_midi_handle_t *midi_handle;
#endif

}; 



#endif /* __JACK_FFADO_DRIVER_H__ */


