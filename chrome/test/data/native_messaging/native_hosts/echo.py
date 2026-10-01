#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple native client in python.
# All this client does is echo the text it receives back at the extension.

import argparse
import base64
import json
import os
import platform
import sys
import struct
import time

def WriteMessage(message):
  try:
    sys.stdout.buffer.write(struct.pack("I", len(message)))
    sys.stdout.buffer.write(message)
    sys.stdout.buffer.flush()
    return True
  except IOError:
    return False


def ParseArgs():
  parser = argparse.ArgumentParser()
  parser.add_argument('--parent-window', type=int)
  parser.add_argument('--reconnect-command')
  parser.add_argument('--native-messaging-connect-id')
  parser.add_argument('--extension-not-installed', action='store_true',
                      default=False)
  parser.add_argument('--invalid-connect-id', action='store_true',
                      default=False)
  parser.add_argument('origin')
  return parser.parse_args()

def Main():
  message_number = 0

  args = ParseArgs()
  caller_url = args.origin

  if sys.argv[1] != args.origin:
    sys.stderr.write(
        "URL of the calling application is not specified as the first arg.\n")
    return 1

  if args.extension_not_installed:
    with open('connect_id.txt', 'w') as f:
      if args.reconnect_command:
        f.write('Unexpected reconnect command: ' + args.reconnect_command)
      else:
        f.write('--connect-id=' + args.native_messaging_connect_id)
    # The timeout in the test is 2 seconds, so sleep for longer than that to
    # force a timeout.
    time.sleep(5)
    return 1

  if args.invalid_connect_id:
    with open('invalid_connect_id.txt', 'w') as f:
      if args.reconnect_command:
        f.write('Unexpected reconnect command: ' + args.reconnect_command)
      else:
        f.write('--invalid-connect-id')
    return 1

  # Verify that the process was started in the correct directory.
  cwd = os.path.realpath(os.getcwd())
  script_path = os.path.dirname(os.path.realpath(sys.argv[0]))
  if cwd.lower() != script_path.lower():
    sys.stderr.write('Native messaging host started in a wrong directory.')
    return 1

  # Verify that --parent-window parameter is correct.
  if platform.system() == 'Windows' and args.parent_window:
    import win32gui
    if not win32gui.IsWindow(args.parent_window):
      sys.stderr.write('Invalid --parent-window.\n')
      return 1

  reconnect_args = json.loads(base64.b64decode(
      args.reconnect_command.encode())) if args.reconnect_command else None

  while 1:
    # Read the message type (first 4 bytes).
    text_length_bytes = sys.stdin.buffer.read(4)

    if len(text_length_bytes) == 0:
      break

    # Read the message length (4 bytes).
    text_length = struct.unpack('i', text_length_bytes)[0]

    # Read the text (JSON object) of the message.
    text = json.loads(sys.stdin.buffer.read(text_length))

    # bigMessage() test sends a special message that is sent to verify that
    # chrome rejects messages that are too big. Try sending a message bigger
    # than 1MB after receiving a message that contains 'bigMessageTest'.
    if 'bigMessageTest' in text:
      text = {"key": "x" * 1024 * 1024}

    # "stopHostTest" verifies that Chrome properly handles the case when the
    # host quits before port is closed. When the test receives response it
    # will try sending second message and it should fail becasue the stdin
    # pipe will be closed at that point.
    if 'stopHostTest' in text:
      # Using os.close() here because sys.stdin.close() doesn't really close
      # the pipe (it just marks it as closed, but doesn't close the file
      # descriptor).
      os.close(sys.stdin.buffer.fileno())
      WriteMessage(b'{"stopped": true }')
      sys.exit(0)

    message_number += 1

    send_invalid_response = 'sendInvalidResponse' in text
    message = None

    if send_invalid_response:
      message = '{'.encode('utf-8')
    else:
      message = json.dumps({
          'id': message_number, 'echo': text, 'caller_url': caller_url,
          'args': reconnect_args, 'connect_id': args.native_messaging_connect_id,
      }).encode('utf-8')

    if not WriteMessage(message):
      break

if __name__ == '__main__':
  sys.exit(Main())
