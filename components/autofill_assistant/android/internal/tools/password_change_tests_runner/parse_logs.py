#!/usr/bin/env python
#
# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The script parses tests logs from adb logcat."""

import argparse
import json
import queue
import re
import subprocess
import sys
import threading


TAG = 'cr_PasswordChange'


class AsynchronousStdoutReader(threading.Thread):
  """Implement asynchronous reading of a process stdout in a separate thread."""

  def __init__(self, p_stdout, queue):
    threading.Thread.__init__(self)
    self._stdout = p_stdout
    self._queue = queue

  def run(self):
    """Read lines from process stdout and put them on the queue."""
    for line in iter(self._stdout.readline, ''):
      self._queue.put(line)


def parse_results(line):
  """Parses and logs event information from logcat."""
  header = re.search(r'cr_PasswordChangeTest: (\[[\w|:| |#]+\])', line).group(1)
  print(header)

  credentials_count = re.search(r'Number of stored credentials: (\d+).', line)
  if not credentials_count:
    # Event does not contain any credentials information.
    # Print an empty line and continue.
    print()
    return

  print('Number of stored credentials: %s' % credentials_count.group(1))

  def build_credential(credential_match):
    return {
        'url': credential_match.group(1),
        'username': credential_match.group(2),
        'password': credential_match.group(3),
    }

  re_all_credentials = re.compile(r'PasswordStoreCredential\{.*?\}')
  re_credential_info = re.compile(
      r'PasswordStoreCredential\{url=(.*?), username=(.*?), password=(.*?)\}')
  credentials = [
      build_credential(re_credential_info.search(credential))
      for credential in re_all_credentials.findall(line)
  ]
  # Print credentials with json format.
  print(json.dumps(credentials, indent=2))
  print()


def main(args):
  parser = argparse.ArgumentParser(
      prog='parse_logs.py',
      description="""The script is intended to parse device logs and display
                                password change events information.
                                """,
  )

  parser.add_argument(
      '--adb-path',
      dest='adb_path',
      required=True,
      help='Full path to adb command.')

  parser.add_argument(
      '--device',
      dest='device_id',
      help='Specify which device to be logged. See \'adb devices\'.')

  parser.add_argument(
      '--clear-prev-logs',
      action='store_true',
      dest='clear_logs',
      help='Clear device logs.')

  # Get the arguments.
  options, _ = parser.parse_known_args(args)

  if options.clear_logs:
    subprocess.call([options.adb_path, 'logcat', '-c'], stdout=subprocess.PIPE)

  def get_adb_command(args):
    if options.device_id:
      return [options.adb_path, '-s', options.device_id] + args
    return [options.adb_path] + args

  # Check adb device is present.
  devices = subprocess.check_output(get_adb_command(['devices'
                                                   ])).strip().splitlines()
  # The first line of `adb devices` just says "List of attached devices".
  if len(devices) == 1:
    print('adb: No emulators found')
    return

  # Check --device is provided if multiple devices are found.
  if len(devices) > 2 and not options.device_id:
    print('adb: Multiple devices found. Please provide --device.')
    return

  logcat_process = subprocess.Popen(get_adb_command(['logcat']),
                             stdout=subprocess.PIPE)

  # Launch the asynchronous reader of the process' stdout.
  stdout_queue = queue.Queue()
  stdout_reader = AsynchronousStdoutReader(logcat_process.stdout, stdout_queue)
  stdout_reader.daemon = True
  stdout_reader.start()

  try:
    while True:
      while not stdout_queue.empty():
        logcat_line = str(stdout_queue.get())
        if TAG in logcat_line:
          parse_results(logcat_line)
  except KeyboardInterrupt:
    print("Terminate")
    logcat_process.terminate()


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
