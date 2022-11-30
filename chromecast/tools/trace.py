#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script was originally written by Alok Priyadarshi (alokp@)
# with some minor local modifications.

import contextlib
import json
import logging
import optparse
import os
import sys
import websocket

from tracinglib import TracingBackend, TracingBackendAndroid, TracingClient

@contextlib.contextmanager
def Connect(options):
  if options.adb_device:
    backend = TracingBackendAndroid(options.adb_device)
  else:
    backend = TracingBackend(options.device, options.port, options.timeout, 0)

  try:
    backend.Connect()
    yield backend
  finally:
    backend.Disconnect()


def GetOutputFilePath(options):
  filepath = os.path.expanduser(options.output) if options.output \
      else os.path.join(os.getcwd(), 'trace.json')

  dirname = os.path.dirname(filepath)
  if dirname:
    if not os.path.exists(dirname):
      os.makedirs(dirname)
  else:
    filepath = os.path.join(os.getcwd(), filepath)

  return filepath


def _CreateOptionParser():
  parser = optparse.OptionParser(description='Record about://tracing profiles '
                                 'from any running instance of Chrome.')
  parser.add_option(
      '-v', '--verbose', help='Verbose logging.', action='store_true')
  parser.add_option(
      '-p', '--port', help='Remote debugging port.', type='int', default=9222)
  parser.add_option(
      '-d', '--device', help='Device ip address.', type='string',
      default='127.0.0.1')
  parser.add_option(
      '-s', '--adb-device', help='Device serial for adb.', type='string')
  parser.add_option(
      '-t', '--timeout', help='Websocket timeout interval.', type='int',
      default=90)

  tracing_opts = optparse.OptionGroup(parser, 'Tracing options')
  tracing_opts.add_option(
      '-c', '--category-filter',
      help='Apply filter to control what category groups should be traced.',
      type='string',
      default='')
  tracing_opts.add_option(
      '--record-continuously',
      help='Keep recording until stopped. The trace buffer is of fixed size '
           'and used as a ring buffer. If this option is omitted then '
           'recording stops when the trace buffer is full.',
      action='store_true')
  tracing_opts.add_option(
      '--systrace',
      help='Enable system tracing.',
      action='store_true',
      dest='systrace',
      default=True)
  parser.add_option_group(tracing_opts)

  output_options = optparse.OptionGroup(parser, 'Output options')
  output_options.add_option(
      '-o', '--output',
      help='Save trace output to file.')
  parser.add_option_group(output_options)

  return parser


def _ProcessOptions(options):
  websocket.enableTrace(options.verbose)


def main():
  parser = _CreateOptionParser()
  options, _args = parser.parse_args()
  _ProcessOptions(options)

  with Connect(options) as tracing_backend:
    tracing_backend.StartTracing(options.category_filter,
                                 options.record_continuously, options.systrace)
    raw_input('Capturing trace. Press Enter to stop...')
    output_path_base = GetOutputFilePath(options)
    output_path = tracing_backend.StopTracing(output_path_base)

  print('Done')
  print('Trace written to file://%s' % output_path)


if __name__ == '__main__':
  sys.exit(main())
