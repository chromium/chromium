# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import http
import logging
import os
import threading

from http.server import SimpleHTTPRequestHandler
from .defines import SRC_DIR

HTTP_DATA_BASEDIR = os.path.join(
    SRC_DIR, 'chrome', 'test', 'data', 'variations', 'http_server')

def start_http_server(
    port: int = 0,
    directory: str = HTTP_DATA_BASEDIR
    ) -> http.server.HTTPServer:
  """Starts a HTTP server serving the given directory."""
  http_server = http.server.HTTPServer(('', port),
      functools.partial(SimpleHTTPRequestHandler, directory=directory))
  logging.info('local http server is running as http://%s:%s',
               http_server.server_name, http_server.server_port)
  threading.Thread(target=http_server.serve_forever).start()
  return http_server
