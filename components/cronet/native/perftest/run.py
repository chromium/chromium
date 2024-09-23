#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script runs an automated Cronet native performance benchmark.

This script:
1. Starts HTTP and QUIC servers on the host machine.
2. Runs benchmark executable.

Prerequisites:
1. quic_server and cronet_native_perf_test have been built for the host machine,
   e.g. via:
     gn gen out/Release --args="is_debug=false"
     autoninja -C out/Release quic_server cronet_native_perf_test
2. sudo apt-get install lighttpd

Invocation:
./run.py

Output:
Benchmark timings are output to /tmp/cronet_perf_test_results.txt

"""

import json
import os
import shutil
import sys
import tempfile

REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'build', 'android'))
import lighttpd_server  # pylint: disable=wrong-import-position
sys.path.append(os.path.join(REPOSITORY_ROOT, 'components'))
from cronet.tools import perf_test_utils  # pylint: disable=wrong-import-position

def main():
  device = perf_test_utils.NativeDevice()
  # Start HTTP server.
  http_server_doc_root = perf_test_utils.GenerateHttpTestResources()
  config_file = tempfile.NamedTemporaryFile()
  http_server = lighttpd_server.LighttpdServer(http_server_doc_root,
      port=perf_test_utils.HTTP_PORT,
      base_config_path=config_file.name)
  perf_test_utils.GenerateLighttpdConfig(config_file, http_server_doc_root,
                                         http_server)
  assert http_server.StartupHttpServer()
  config_file.close()
  # Start QUIC server.
  quic_server_doc_root = perf_test_utils.GenerateQuicTestResources(device)
  quic_server = perf_test_utils.QuicServer(quic_server_doc_root)
  quic_server.StartupQuicServer(device)
  # Run test
  os.environ['LD_LIBRARY_PATH'] = perf_test_utils.BUILD_DIR
  device.RunShellCommand(
      [os.path.join(perf_test_utils.BUILD_DIR, 'cronet_native_perf_test'),
          json.dumps(perf_test_utils.GetConfig(device))],
      check_return=True)
  # Shutdown.
  quic_server.ShutdownQuicServer()
  shutil.rmtree(quic_server_doc_root)
  http_server.ShutdownHttpServer()
  shutil.rmtree(http_server_doc_root)


if __name__ == '__main__':
  main()
