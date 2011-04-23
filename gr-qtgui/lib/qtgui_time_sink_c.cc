/* -*- c++ -*- */
/*
 * Copyright 2011 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qtgui_time_sink_c.h>
#include <gr_io_signature.h>
#include <string.h>

#include <QTimer>

qtgui_time_sink_c_sptr
qtgui_make_time_sink_c (int size, double bw,
			const std::string &name,
			int nconnections,
			QWidget *parent)
{
  return gnuradio::get_initial_sptr(new qtgui_time_sink_c (size, bw, name,
							   nconnections, parent));
}

qtgui_time_sink_c::qtgui_time_sink_c (int size, double bw,
				      const std::string &name,
				      int nconnections,
				      QWidget *parent)
  : gr_block ("time_sink_c",
	      gr_make_io_signature (nconnections, nconnections, sizeof(gr_complex)),
	      gr_make_io_signature (0, 0, 0)),
    d_size(size), d_bandwidth(bw), d_name(name),
    d_nconnections(2*nconnections), d_parent(parent)
{
  d_main_gui = NULL;

  d_index = 0;

  for(int i = 0; i < d_nconnections; i++) {
    d_residbufs.push_back(new double[d_size]);
  }

  initialize();
}

qtgui_time_sink_c::~qtgui_time_sink_c()
{
  // d_main_gui is a qwidget destroyed with its parent
  for(int i = 0; i < d_nconnections; i++) {
    delete [] d_residbufs[i];
  }
}

void
qtgui_time_sink_c::forecast(int noutput_items, gr_vector_int &ninput_items_required)
{
  unsigned int ninputs = ninput_items_required.size();
  for (unsigned int i = 0; i < ninputs; i++) {
    ninput_items_required[i] = std::min(d_size, 8191);
  }
}

void
qtgui_time_sink_c::initialize()
{
  if(qApp != NULL) {
    d_qApplication = qApp;
  }
  else {
    int argc=0;
    char **argv = NULL;
    d_qApplication = new QApplication(argc, argv);
  }

  d_main_gui = new TimeDisplayForm(d_nconnections, d_parent);

  // initialize update time to 10 times a second
  set_update_time(0.1);
}


void
qtgui_time_sink_c::exec_()
{
  d_qApplication->exec();
}

QWidget*
qtgui_time_sink_c::qwidget()
{
  return d_main_gui;
}

PyObject*
qtgui_time_sink_c::pyqwidget()
{
  PyObject *w = PyLong_FromVoidPtr((void*)d_main_gui);
  PyObject *retarg = Py_BuildValue("N", w);
  return retarg;
}

void
qtgui_time_sink_c::set_time_domain_axis(double min, double max)
{
  d_main_gui->SetTimeDomainAxis(min, max);
}

void
qtgui_time_sink_c::set_update_time(double t)
{
  d_update_time = t;
  d_main_gui->SetUpdateTime(d_update_time);
}

int
qtgui_time_sink_c::general_work (int noutput_items,
				 gr_vector_int &ninput_items,
				 gr_vector_const_void_star &input_items,
				 gr_vector_void_star &output_items)
{
  int n=0, j=0;
  const gr_complex *in = (const gr_complex*)input_items[0];

  for(int i=0; i < noutput_items; i+=d_size) {
    unsigned int datasize = noutput_items - i;
    unsigned int resid = d_size-d_index;
    
    // If we have enough input for one full plot, do it
    if(datasize >= resid) {
      d_current_time = get_highres_clock();
      
      // Fill up residbufs with d_size number of items
      for(n = 0; n < d_nconnections; n+=2) {
	for(unsigned int k = 0; k < resid; k++) {
	  d_residbufs[n][d_index+k] = in[j+k].real();
	  d_residbufs[n+1][d_index+k] = in[j+k].imag();
	}
      }	

      // Update the plot if its time
      if(diff_timespec(d_current_time, d_last_time) > d_update_time) {
	d_last_time = d_current_time;
	d_qApplication->postEvent(d_main_gui,
				  new TimeUpdateEvent(d_residbufs, d_size));	
      }
      
      d_index = 0;
      j += resid;
    }
    // Otherwise, copy what we received into the residbufs for next time
    else {
      for(n = 0; n < d_nconnections; n+=2) {
	for(unsigned int k = 0; k < resid; k++) {
	    d_residbufs[n][d_index+k] = in[j+k].real();
	    d_residbufs[n+1][d_index+k] = in[j+k].imag();
	}
      }
      d_index += datasize;
      j += datasize;
    }   
  }
  
  consume_each(j);
  return j;
}
