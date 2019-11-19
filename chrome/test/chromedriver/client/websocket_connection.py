# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import json
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

class WebSocketCommands:
  CREATE_WEBSOCKET = \
    '/session/:sessionId/chromium/create_websocket'
  SEND_OVER_WEBSOCKET = \
    '/session/:sessionId/chromium/send_command_from_websocket'

class WebSocketConnection(object):
  def __init__(self, server_url, session_id):
    self._server_url = server_url.replace('http', 'ws')
    self._session_id = session_id
    self._command_id = -1
    cmd_params = {'sessionId': session_id}
    path = CommandExecutor.CreatePath(
      WebSocketCommands.CREATE_WEBSOCKET, cmd_params)
    self._websocket = websocket.create_connection(self._server_url + path)

  def SendCommand(self, cmd_params):
    cmd_params['id'] = self._command_id
    self._command_id -= 1
    self._websocket.send(json.dumps(cmd_params))
