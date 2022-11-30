# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import time
from command_executor import CommandExecutor

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
sys.path.insert(1, _PARENT_DIR)
import chrome_paths
sys.path.remove(_PARENT_DIR)
sys.path.insert(0,os.path.join(chrome_paths.GetSrc(), 'third_party',
                               'catapult', 'telemetry', 'third_party',
                               'websocket-client'))
import websocket

from websocket import WebSocketConnectionClosedException as InternalWebSocketConnectionClosedException
from websocket import WebSocketTimeoutException as InternalWebSocketTimeoutException
from exceptions import WebSocketConnectionClosedException
from exceptions import WebSocketTimeoutException

class WebSocketCommands:
  CREATE_WEBSOCKET = \
    '/session/:sessionId'
  SEND_OVER_WEBSOCKET = \
    '/session/:sessionId/chromium/send_command_from_websocket'

class WebSocketConnection(object):
  def __init__(self, server_url, session_id):
    self._server_url = server_url.replace('http', 'ws')
    self._session_id = session_id
    self._command_id = 0
    cmd_params = {'sessionId': session_id}
    path = CommandExecutor.CreatePath(
      WebSocketCommands.CREATE_WEBSOCKET, cmd_params)
    self._websocket = websocket.create_connection(self._server_url + path)
    self._responses = {}

  def SendCommand(self, cmd_params):
    self._command_id = self._command_id + 1
    cmd_params['id'] = self._command_id
    try:
      self._websocket.send(json.dumps(cmd_params))
    except InternalWebSocketTimeoutException:
      raise WebSocketTimeoutException()
    return self._command_id

  def WaitForResponse(self, command_id):
    if command_id in self._responses:
      msg = self._responses[command_id]
      del self._responses[command_id]
      return msg

    start = time.monotonic()
    timeout = self.GetTimeout()

    try:
      while True:
        msg = json.loads(self._websocket.recv())
        if 'id' in msg:
          if msg['id'] == command_id:
            return msg
          elif msg['id'] >= 0:
            self._responses[msg['id']] = msg
        if start + timeout <= time.monotonic():
          raise TimeoutError()
    except InternalWebSocketConnectionClosedException:
      raise WebSocketConnectionClosedException()
    except InternalWebSocketTimeoutException:
      raise WebSocketTimeoutException()

  def Close(self):
    self._websocket.close();

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def GetTimeout(self):
    return self._websocket.gettimeout()

  def SetTimeout(self, timeout_seconds):
    self._websocket.settimeout(timeout_seconds)
