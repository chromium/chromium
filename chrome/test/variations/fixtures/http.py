# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import http
import logging
import os
import threading

import pytest

from chrome.test.variations.test_utils import SRC_DIR
from http.server import SimpleHTTPRequestHandler

HTTP_DATA_BASEDIR = os.path.join(
    SRC_DIR, 'chrome', 'test', 'data', 'variations', 'http_server')

def _start_http_server(
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

@pytest.fixture(scope='session')
def local_http_server() -> http.server.HTTPServer:
  """Starts and returns a http server."""
  http_server = _start_http_server()
  yield http_server
  http_server.shutdown()
