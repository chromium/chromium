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
from exceptions import ChromeDriverException
from exceptions import EXCEPTION_MAP

class WebSocketCommands:
  ATTACH_WEBSOCKET_TO_SESSION = \
    '/session/:sessionId'
  CREATE_UNBOUND_WEBSOCKET = \
    '/session'
  SEND_OVER_WEBSOCKET = \
    '/session/:sessionId/chromium/send_command_from_websocket'

class WebSocketConnection(object):
  def __init__(self, server_url, session_id=None):
    self._server_url = server_url.replace('http', 'ws')
    self._session_id = session_id
    self._command_id = 0
    if session_id is None:
      path = CommandExecutor.CreatePath(
          WebSocketCommands.CREATE_UNBOUND_WEBSOCKET, {})
    else:
      path = CommandExecutor.CreatePath(
        WebSocketCommands.ATTACH_WEBSOCKET_TO_SESSION,
        {'sessionId': session_id})
    self._websocket = websocket.create_connection(self._server_url + path)
    self._responses = {}
    self._events = []

  def PostCommand(self, cmd_params):
    if 'id' not in cmd_params:
      self._command_id = self._command_id + 1
      cmd_params['id'] = self._command_id
    try:
      self._websocket.send(json.dumps(cmd_params))
    except InternalWebSocketTimeoutException:
      raise WebSocketTimeoutException()
    except InternalWebSocketConnectionClosedException:
      raise WebSocketConnectionClosedException()
    except ConnectionError:
      raise WebSocketConnectionClosedException()
    return cmd_params['id']

  def SendCommand(self, cmd_params, channel=None, timeout=None):
    cmd_id = self.PostCommand(cmd_params)
    return self.WaitForResponse(cmd_id, channel=channel, timeout=timeout)

  def SendCommandRaw(self, cmd_params, channel=None, timeout=None):
    cmd_id = self.PostCommand(cmd_params)
    return self.WaitForResponseRaw(cmd_id, channel=channel, timeout=timeout)

  def TakeEvents(self):
    result = self._events
    self._events = []
    return result

  def TryGetResponse(self, command_id, channel=None):
    if channel not in self._responses:
      return None
    return self._responses[channel].get(command_id, None)

  def _ExceptionForResponse(self, response):
    error = response['error']
    msg = response['message']
    cmd_id = response.get('id', None)
    ret = EXCEPTION_MAP.get(error, ChromeDriverException)(msg)
    if cmd_id is not None and ret is not None:
      ret.id = cmd_id
    return ret

  def WaitForResponse(self, command_id, channel=None, timeout=None):
    response = self.WaitForResponseRaw(command_id,
                                       channel=channel,
                                       timeout=timeout)
    if response['type'] == 'error':
      raise self._ExceptionForResponse(response)
    if response['type'] == 'success':
      return response['result']
    # Unexpected response type. Return it as it is.
    return response

  def _IsExpectedResponse(self, message, expected_id, expected_channel):
    actual_channel = message.get('goog:channel', None)
    actual_id = message.get('id', None)
    return actual_id == expected_id and actual_channel == expected_channel

  def WaitForResponseRaw(self, command_id, channel=None, timeout=None):
    if (channel not in self._responses
        or command_id not in self._responses[channel]):
      self._WaitForMessage(
          lambda message: self._IsExpectedResponse(
              message,
              command_id,
              channel), timeout)

    msg = self._responses[channel][command_id]
    del self._responses[channel][command_id]
    return msg

  def _IsExpectedEvent(self, message, event_name, channel):
    return (message.get('id') == None
            and message.get('method') == event_name
            and message.get('goog:channel') == channel)

  def WaitForEvent(self, event_name, channel=None, timeout=None):
    return self._WaitForMessage(
        lambda message: self._IsExpectedEvent(
            message,
            event_name,
            channel), timeout)

  def _WaitForMessage(self, predicate, timeout=None):
    """
    Wait for a message matching the predicate.

    timeout: timeout in seconds, fractional
    """
    start = time.monotonic()
    if timeout == None:
      timeout = self.GetTimeout()

    try:
      while True:
        msg = json.loads(self._websocket.recv())
        if 'id' in msg:
          resp_channel = msg.get('goog:channel', None)
          if resp_channel not in self._responses:
            self._responses[resp_channel] = {}
          self._responses[resp_channel][msg['id']] = msg
        else: # event
          self._events.append(msg)
        if (predicate(msg)):
          return msg
        if start + timeout <= time.monotonic():
          raise WebSocketTimeoutException()
    except InternalWebSocketConnectionClosedException:
      raise WebSocketConnectionClosedException()
    except ConnectionError:
      raise WebSocketConnectionClosedException()
    except InternalWebSocketTimeoutException:
      raise WebSocketTimeoutException()

  def Close(self):
    self._websocket.close()

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def GetTimeout(self):
    """ Get the websocket timeout(second)"""
    return self._websocket.gettimeout()

  def SetTimeout(self, timeout_seconds):
    """ Set the websocket timeout(second)"""
    self._websocket.settimeout(timeout_seconds)
