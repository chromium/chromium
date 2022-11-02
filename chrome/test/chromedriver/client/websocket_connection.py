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
    self._events = []

  def SendCommand(self, cmd_params):
    if 'id' not in cmd_params:
      self._command_id = self._command_id + 1
      cmd_params['id'] = self._command_id
    try:
      self._websocket.send(json.dumps(cmd_params))
    except InternalWebSocketTimeoutException:
      raise WebSocketTimeoutException()
    return cmd_params['id']

  def TakeEvents(self):
    result = self._events;
    self._events = []
    return result;

  def TryGetResponse(self, command_id, channel = None):
    if channel not in self._responses:
      return None
    if command_id not in self._responses[channel]:
      return None
    return self._responses[channel][command_id]

  def WaitForResponse(self, command_id, channel = None):
    if channel in self._responses:
      if command_id in self._responses[channel]:
        msg = self._responses[channel][command_id]
        del self._responses[channel][command_id]
        return msg

    start = time.monotonic()
    timeout = self.GetTimeout()

    try:
      while True:
        msg = json.loads(self._websocket.recv())
        if 'id' in msg:
          resp_channel = None
          if 'channel' in msg:
            resp_channel = msg['channel']
          if msg['id'] == command_id and resp_channel == channel:
            return msg
          elif msg['id'] >= 0:
            if resp_channel not in self._responses:
              self._responses[resp_channel] = {}
            self._responses[resp_channel][msg['id']] = msg
        else: # event
          self._events.append(msg)
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
