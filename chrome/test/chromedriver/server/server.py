# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import os
import socket
import subprocess
import threading
import time
import urllib2

def terminate_process(proc):
  """Terminates the process.

  If an error occurs ignore it, just print out a message.

  Args:
    proc: A subprocess.
  """
  try:
    proc.terminate()
  except OSError as ex:
    print 'Error while killing a process: %s' % ex


class Server(object):
  """A running ChromeDriver server."""

  def __init__(self, exe_path, log_path=None, verbose=True,
               replayable=False, devtools_replay_path=None):
    """Starts the ChromeDriver server and waits for it to be ready.

    Args:
      exe_path: path to the ChromeDriver executable
      log_path: path to the log file
      verbose: make the logged data verbose
      replayable: don't truncate strings in log to make the session replayable
      devtools_replay_path: replay devtools events from the log at this path
    Raises:
      RuntimeError: if ChromeDriver fails to start
    """
    if not os.path.exists(exe_path):
      raise RuntimeError('ChromeDriver exe not found at: ' + exe_path)

    port = self._FindOpenPort()
    chromedriver_args = [exe_path, '--port=%d' % port]
    if log_path:
      chromedriver_args.extend(['--log-path=%s' % log_path])
      chromedriver_args.extend(['--append-log'])
      chromedriver_args.extend(['--readable-timestamp'])
      if verbose:
        chromedriver_args.extend(['--verbose',
                                  '--vmodule=*/chrome/test/chromedriver/*=3'])
      if replayable:
        chromedriver_args.extend(['--replayable'])

    if devtools_replay_path:
      chromedriver_args.extend(['--devtools-replay=%s' % devtools_replay_path])

    self._process = subprocess.Popen(chromedriver_args)
    self._host = '127.0.0.1'
    self._port = port
    self._url = 'http://%s:%d' % (self._host, port)
    if self._process is None:
      raise RuntimeError('ChromeDriver server cannot be started')

    max_time = time.time() + 20
    while not self.IsRunning():
      if time.time() > max_time:
        self._process.poll()
        if self._process.returncode is None:
          print 'ChromeDriver process still running, but not responding'
        else:
          print ('ChromeDriver process exited with return code %d'
                 % self._process.returncode)
        self._process.terminate()
        raise RuntimeError('ChromeDriver server did not start')
      time.sleep(0.1)

    atexit.register(self.Kill)

  def _FindOpenPort(self):
    for port in range(9500, 10000):
      try:
        socket.create_connection(('127.0.0.1', port), 0.2).close()
      except socket.error:
        return port
    raise RuntimeError('Cannot find open port to launch ChromeDriver')

  def GetUrl(self):
    return self._url

  def GetHost(self):
    return self._host

  def GetPort(self):
    return self._port

  def IsRunning(self):
    """Returns whether the server is up and running."""
    try:
      urllib2.urlopen(self.GetUrl() + '/status')
      return True
    except urllib2.URLError:
      return False

  def Kill(self):
    """Kills the ChromeDriver server, if it is running."""
    if self._process is None:
      return

    try:
      urllib2.urlopen(self.GetUrl() + '/shutdown', timeout=10).close()
    except:
      self._process.terminate()
    timer = threading.Timer(5, terminate_process, [self._process])
    timer.start()
    self._process.wait()
    timer.cancel()
    self._process = None
