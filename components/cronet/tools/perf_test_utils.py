#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for Cronet performance tests."""

import logging
import os
import posixpath
import subprocess
import tempfile
from time import sleep

from cronet.tools import android_rndis_forwarder


REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))
BUILD_TYPE = 'Release'
BUILD_DIR = os.path.join(REPOSITORY_ROOT, 'out', BUILD_TYPE)
QUIC_SERVER = os.path.join(BUILD_DIR, 'quic_server')
CERT_PATH = os.path.join('net', 'data', 'ssl', 'certificates')
QUIC_CERT_DIR = os.path.join(REPOSITORY_ROOT, CERT_PATH)
QUIC_CERT_HOST = 'test.example.com'
QUIC_CERT_FILENAME = 'quic-chain.pem'
QUIC_CERT = os.path.join(QUIC_CERT_DIR, QUIC_CERT_FILENAME)
QUIC_KEY = os.path.join(QUIC_CERT_DIR, 'quic-leaf-cert.key')
APP_APK = os.path.join(BUILD_DIR, 'apks', 'CronetPerfTest.apk')
APP_PACKAGE = 'org.chromium.net'
APP_ACTIVITY = '.CronetPerfTestActivity'
APP_ACTION = 'android.intent.action.MAIN'
HTTP_PORT = None  # Value will be overridden by DEFAULT_BENCHMARK_CONFIG.
# TODO(pauljensen): Consider whether we can avoid loading this
# DEFAULT_BENCHMARK_CONFIG dict into globals.
DEFAULT_BENCHMARK_CONFIG = {
  # Control various metric recording for further investigation.
  'CAPTURE_NETLOG': False,
  'CAPTURE_TRACE': False,
  'CAPTURE_SAMPLED_TRACE': False,
  # While running Cronet Async API benchmarks, indicate if callbacks should be
  # run on network thread rather than posted back to caller thread.  This allows
  # measuring if thread-hopping overhead is significant.
  'CRONET_ASYNC_USE_NETWORK_THREAD': False,
  # A small resource for device to fetch from host.
  'SMALL_RESOURCE': 'small.html',
  'SMALL_RESOURCE_SIZE': 26,
  # Number of times to fetch SMALL_RESOURCE.
  'SMALL_ITERATIONS': 1000,
  # A large resource for device to fetch from host.
  'LARGE_RESOURCE': 'large.html',
  'LARGE_RESOURCE_SIZE': 10000026,
  # Number of times to fetch LARGE_RESOURCE.
  'LARGE_ITERATIONS': 4,
  # Ports of HTTP and QUIC servers on host.
  'HTTP_PORT': 9000,
  'QUIC_PORT': 9001,
  # Maximum read/write buffer size to use.
  'MAX_BUFFER_SIZE': 16384,
  'HOST': QUIC_CERT_HOST,
  'QUIC_CERT_FILE': QUIC_CERT_FILENAME,
}
# Add benchmark config to global state for easy access.
globals().update(DEFAULT_BENCHMARK_CONFIG)
# Pylint doesn't really interpret the file, so it won't find the definitions
# added from DEFAULT_BENCHMARK_CONFIG, so suppress the undefined variable
# warning.
#pylint: disable=undefined-variable

class NativeDevice(object):
  def GetExternalStoragePath(self):
    return '/tmp'

  def RunShellCommand(self, cmd, check_return=False):
    if check_return:
      subprocess.check_call(cmd)
    else:
      subprocess.call(cmd)

  def WriteFile(self, path, data):
    with open(path, 'w') as f:
      f.write(data)

def GetConfig(device):
  config = DEFAULT_BENCHMARK_CONFIG
  config['HOST_IP'] = GetServersHost(device)
  if isinstance(device, NativeDevice):
    config['RESULTS_FILE'] = '/tmp/cronet_perf_test_results.txt'
    config['DONE_FILE'] = '/tmp/cronet_perf_test_done.txt'
  else:
    # An on-device file containing benchmark timings.  Written by benchmark app.
    config['RESULTS_FILE'] = '/data/data/' + APP_PACKAGE + '/results.txt'
    # An on-device file whose presence indicates benchmark app has terminated.
    config['DONE_FILE'] = '/data/data/' + APP_PACKAGE + '/done.txt'
  return config


def GetAndroidRndisConfig(device):
  return android_rndis_forwarder.AndroidRndisConfigurator(device)


def GetServersHost(device):
  if isinstance(device, NativeDevice):
    return '127.0.0.1'
  return GetAndroidRndisConfig(device).host_ip


def GetHttpServerURL(device, resource):
  return 'http://%s:%d/%s' % (GetServersHost(device), HTTP_PORT, resource)


class QuicServer(object):

  def __init__(self, quic_server_doc_root):
    self._process = None
    self._quic_server_doc_root = quic_server_doc_root

  def StartupQuicServer(self, device):
    cmd = [QUIC_SERVER,
           '--quic_response_cache_dir=%s' % self._quic_server_doc_root,
           '--certificate_file=%s' % QUIC_CERT,
           '--key_file=%s' % QUIC_KEY,
           '--port=%d' % QUIC_PORT]
    logging.info("Starting Quic Server: %s", cmd)
    self._process = subprocess.Popen(cmd)
    assert self._process != None
    # Wait for quic_server to start serving.
    waited_s = 0
    while subprocess.call(['lsof', '-i', 'udp:%d' % QUIC_PORT, '-p',
                           '%d' % self._process.pid],
                          stdout=open(os.devnull, 'w')) != 0:
      sleep(0.1)
      waited_s += 0.1
      assert waited_s < 5, "quic_server failed to start after %fs" % waited_s
    # Push certificate to device.
    cert = open(QUIC_CERT, 'r').read()
    device_cert_path = posixpath.join(
        device.GetExternalStoragePath(), 'chromium_tests_root', CERT_PATH)
    device.RunShellCommand(['mkdir', '-p', device_cert_path], check_return=True)
    device.WriteFile(os.path.join(device_cert_path, QUIC_CERT_FILENAME), cert)

  def ShutdownQuicServer(self):
    if self._process:
      self._process.terminate()


def GenerateHttpTestResources():
  http_server_doc_root = tempfile.mkdtemp()
  # Create a small test file to serve.
  small_file_name = os.path.join(http_server_doc_root, SMALL_RESOURCE)
  small_file = open(small_file_name, 'wb')
  small_file.write('<html><body></body></html>');
  small_file.close()
  assert SMALL_RESOURCE_SIZE == os.path.getsize(small_file_name)
  # Create a large (10MB) test file to serve.
  large_file_name = os.path.join(http_server_doc_root, LARGE_RESOURCE)
  large_file = open(large_file_name, 'wb')
  large_file.write('<html><body>');
  for _ in range(0, 1000000):
    large_file.write('1234567890');
  large_file.write('</body></html>');
  large_file.close()
  assert LARGE_RESOURCE_SIZE == os.path.getsize(large_file_name)
  return http_server_doc_root


def GenerateQuicTestResources(device):
  quic_server_doc_root = tempfile.mkdtemp()
  # Use wget to build up fake QUIC in-memory cache dir for serving.
  # quic_server expects the dir/file layout that wget produces.
  for resource in [SMALL_RESOURCE, LARGE_RESOURCE]:
    assert subprocess.Popen(['wget', '-p', '-q', '--save-headers',
                             GetHttpServerURL(device, resource)],
                            cwd=quic_server_doc_root).wait() == 0
  # wget places results in host:port directory.  Adjust for QUIC port.
  os.rename(os.path.join(quic_server_doc_root,
                         "%s:%d" % (GetServersHost(device), HTTP_PORT)),
            os.path.join(quic_server_doc_root,
                         "%s:%d" % (QUIC_CERT_HOST, QUIC_PORT)))
  return quic_server_doc_root


def GenerateLighttpdConfig(config_file, http_server_doc_root, http_server):
  # Must create customized config file to allow overriding the server.bind
  # setting.
  config_file.write('server.document-root = "%s"\n' % http_server_doc_root)
  config_file.write('server.port = %d\n' % HTTP_PORT)
  # These lines are added so lighttpd_server.py's internal test succeeds.
  config_file.write('server.tag = "%s"\n' % http_server.server_tag)
  config_file.write('server.pid-file = "%s"\n' % http_server.pid_file)
  config_file.write('dir-listing.activate = "enable"\n')
  config_file.flush()
