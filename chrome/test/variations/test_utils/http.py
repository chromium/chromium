# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import http
import logging
import os
import threading

from http.server import SimpleHTTPRequestHandler

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 4)))

HTTP_DATA_BASEDIR = os.path.join(
    SRC_DIR, 'chrome', 'test', 'data', 'chromedriver')

def start_http_server(port:int = 8000,
                      directory: str = None) -> http.server.HTTPServer:
  """Starts a HTTP server serving the given directory."""
  if directory is None:
    directory = HTTP_DATA_BASEDIR
  http_server = http.server.HTTPServer(('', port),
      functools.partial(SimpleHTTPRequestHandler, directory=directory))
  logging.info('local http server is running as http://%s:%s',
               http_server.server_name, http_server.server_port)
  threading.Thread(target=http_server.serve_forever).start()
  return http_server
