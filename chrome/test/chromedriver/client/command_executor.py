# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import httplib
import json
from urlparse import urlparse

class _Method(object):
  GET = 'GET'
  POST = 'POST'
  DELETE = 'DELETE'


class Command(object):
  NEW_SESSION = (_Method.POST, '/session')
  GET_SESSION_CAPABILITIES = (_Method.GET, '/session/:sessionId')
  GET_SESSIONS = (_Method.GET, '/sessions')
  QUIT = (_Method.DELETE, '/session/:sessionId')
  GET_CURRENT_WINDOW_HANDLE = (_Method.GET, '/session/:sessionId/window')
  GET_WINDOW_HANDLES = (_Method.GET, '/session/:sessionId/window/handles')
  GET = (_Method.POST, '/session/:sessionId/url')
  GET_ALERT = (_Method.GET, '/session/:sessionId/alert')
  DISMISS_ALERT = (_Method.POST, '/session/:sessionId/alert/dismiss')
  ACCEPT_ALERT = (_Method.POST, '/session/:sessionId/alert/accept')
  GET_ALERT_TEXT = (_Method.GET, '/session/:sessionId/alert/text')
  SET_ALERT_VALUE = (_Method.POST, '/session/:sessionId/alert/text')
  GO_FORWARD = (_Method.POST, '/session/:sessionId/forward')
  GO_BACK = (_Method.POST, '/session/:sessionId/back')
  REFRESH = (_Method.POST, '/session/:sessionId/refresh')
  EXECUTE_SCRIPT = (_Method.POST, '/session/:sessionId/execute/sync')
  EXECUTE_ASYNC_SCRIPT = (_Method.POST, '/session/:sessionId/execute/async')
  LAUNCH_APP = (_Method.POST, '/session/:sessionId/chromium/launch_app')
  GET_CURRENT_URL = (_Method.GET, '/session/:sessionId/url')
  GET_TITLE = (_Method.GET, '/session/:sessionId/title')
  GET_PAGE_SOURCE = (_Method.GET, '/session/:sessionId/source')
  SCREENSHOT = (_Method.GET, '/session/:sessionId/screenshot')
  ELEMENT_SCREENSHOT = (
      _Method.GET, '/session/:sessionId/element/:id/screenshot')
  SET_BROWSER_VISIBLE = (_Method.POST, '/session/:sessionId/visible')
  IS_BROWSER_VISIBLE = (_Method.GET, '/session/:sessionId/visible')
  FIND_ELEMENT = (_Method.POST, '/session/:sessionId/element')
  FIND_ELEMENTS = (_Method.POST, '/session/:sessionId/elements')
  GET_ACTIVE_ELEMENT = (_Method.GET, '/session/:sessionId/element/active')
  FIND_CHILD_ELEMENT = (_Method.POST, '/session/:sessionId/element/:id/element')
  FIND_CHILD_ELEMENTS = (
      _Method.POST, '/session/:sessionId/element/:id/elements')
  CLICK_ELEMENT = (_Method.POST, '/session/:sessionId/element/:id/click')
  CLEAR_ELEMENT = (_Method.POST, '/session/:sessionId/element/:id/clear')
  SUBMIT_ELEMENT = (_Method.POST, '/session/:sessionId/element/:id/submit')
  GET_ELEMENT_TEXT = (_Method.GET, '/session/:sessionId/element/:id/text')
  SEND_KEYS_TO_ELEMENT = (_Method.POST, '/session/:sessionId/element/:id/value')
  UPLOAD_FILE = (_Method.POST, '/session/:sessionId/file')
  GET_ELEMENT_VALUE = (_Method.GET, '/session/:sessionId/element/:id/value')
  GET_ELEMENT_TAG_NAME = (_Method.GET, '/session/:sessionId/element/:id/name')
  IS_ELEMENT_SELECTED = (
      _Method.GET, '/session/:sessionId/element/:id/selected')
  IS_ELEMENT_ENABLED = (_Method.GET, '/session/:sessionId/element/:id/enabled')
  IS_ELEMENT_DISPLAYED = (
      _Method.GET, '/session/:sessionId/element/:id/displayed')
  HOVER_OVER_ELEMENT = (_Method.POST, '/session/:sessionId/element/:id/hover')
  GET_ELEMENT_LOCATION = (
      _Method.GET, '/session/:sessionId/element/:id/location')
  GET_ELEMENT_RECT = (
      _Method.GET, '/session/:sessionId/element/:id/rect')
  GET_ELEMENT_LOCATION_ONCE_SCROLLED_INTO_VIEW = (
      _Method.GET, '/session/:sessionId/element/:id/location_in_view')
  GET_ELEMENT_SIZE = (_Method.GET, '/session/:sessionId/element/:id/size')
  GET_ELEMENT_ATTRIBUTE = (
      _Method.GET, '/session/:sessionId/element/:id/attribute/:name')
  GET_ELEMENT_PROPERTY = (
      _Method.GET, '/session/:sessionId/element/:id/property/:name')
  ELEMENT_EQUALS = (
      _Method.GET, '/session/:sessionId/element/:id/equals/:other')
  GET_COOKIES = (_Method.GET, '/session/:sessionId/cookie')
  GET_NAMED_COOKIE = (_Method.GET, '/session/:sessionId/cookie/:name')
  ADD_COOKIE = (_Method.POST, '/session/:sessionId/cookie')
  DELETE_ALL_COOKIES = (_Method.DELETE, '/session/:sessionId/cookie')
  DELETE_COOKIE = (_Method.DELETE, '/session/:sessionId/cookie/:name')
  SWITCH_TO_FRAME = (_Method.POST, '/session/:sessionId/frame')
  SWITCH_TO_PARENT_FRAME = (_Method.POST, '/session/:sessionId/frame/parent')
  SWITCH_TO_WINDOW = (_Method.POST, '/session/:sessionId/window')
  GET_WINDOW_RECT = (_Method.GET, '/session/:sessionId/window/rect')
  GET_WINDOW_SIZE = (
      _Method.GET, '/session/:sessionId/window/:windowHandle/size')
  GET_WINDOW_POSITION = (
      _Method.GET, '/session/:sessionId/window/:windowHandle/position')
  SET_WINDOW_SIZE = (
      _Method.POST, '/session/:sessionId/window/:windowHandle/size')
  SET_WINDOW_POSITION = (
      _Method.POST, '/session/:sessionId/window/:windowHandle/position')
  SET_WINDOW_RECT = (
      _Method.POST, '/session/:sessionId/window/rect')
  MAXIMIZE_WINDOW = (
      _Method.POST, '/session/:sessionId/window/:windowHandle/maximize')
  MINIMIZE_WINDOW = (
      _Method.POST, '/session/:sessionId/window/:windowHandle/minimize')
  FULLSCREEN_WINDOW = (
      _Method.POST, '/session/:sessionId/window/fullscreen')
  CLOSE = (_Method.DELETE, '/session/:sessionId/window')
  DRAG_ELEMENT = (_Method.POST, '/session/:sessionId/element/:id/drag')
  GET_ELEMENT_VALUE_OF_CSS_PROPERTY = (
      _Method.GET, '/session/:sessionId/element/:id/css/:propertyName')
  IMPLICITLY_WAIT = (
      _Method.POST, '/session/:sessionId/timeouts/implicit_wait')
  SET_SCRIPT_TIMEOUT = (
      _Method.POST, '/session/:sessionId/timeouts/async_script')
  SET_TIMEOUTS = (_Method.POST, '/session/:sessionId/timeouts')
  GET_TIMEOUTS = (_Method.GET, '/session/:sessionId/timeouts')
  EXECUTE_SQL = (_Method.POST, '/session/:sessionId/execute_sql')
  GET_LOCATION = (_Method.GET, '/session/:sessionId/location')
  SET_LOCATION = (_Method.POST, '/session/:sessionId/location')
  GET_NETWORK_CONNECTION = (
     _Method.GET, '/session/:sessionId/network_connection')
  GET_NETWORK_CONDITIONS = (
      _Method.GET, '/session/:sessionId/chromium/network_conditions')
  SET_NETWORK_CONDITIONS = (
      _Method.POST, '/session/:sessionId/chromium/network_conditions')
  DELETE_NETWORK_CONDITIONS = (
      _Method.DELETE, '/session/:sessionId/chromium/network_conditions')
  GET_STATUS = (_Method.GET, '/session/:sessionId/application_cache/status')
  IS_BROWSER_ONLINE = (_Method.GET, '/session/:sessionId/browser_connection')
  SET_BROWSER_ONLINE = (_Method.POST, '/session/:sessionId/browser_connection')
  GET_LOCAL_STORAGE_ITEM = (
      _Method.GET, '/session/:sessionId/local_storage/key/:key')
  REMOVE_LOCAL_STORAGE_ITEM = (
      _Method.DELETE, '/session/:sessionId/local_storage/key/:key')
  GET_LOCAL_STORAGE_KEYS = (_Method.GET, '/session/:sessionId/local_storage')
  SET_LOCAL_STORAGE_ITEM = (_Method.POST, '/session/:sessionId/local_storage')
  CLEAR_LOCAL_STORAGE = (_Method.DELETE, '/session/:sessionId/local_storage')
  GET_LOCAL_STORAGE_SIZE = (
      _Method.GET, '/session/:sessionId/local_storage/size')
  GET_SESSION_STORAGE_ITEM = (
      _Method.GET, '/session/:sessionId/session_storage/key/:key')
  REMOVE_SESSION_STORAGE_ITEM = (
      _Method.DELETE, '/session/:sessionId/session_storage/key/:key')
  GET_SESSION_STORAGE_KEY = (_Method.GET, '/session/:sessionId/session_storage')
  SET_SESSION_STORAGE_ITEM = (
      _Method.POST, '/session/:sessionId/session_storage')
  CLEAR_SESSION_STORAGE = (
      _Method.DELETE, '/session/:sessionId/session_storage')
  GET_SESSION_STORAGE_SIZE = (
      _Method.GET, '/session/:sessionId/session_storage/size')
  GET_SCREEN_ORIENTATION = (_Method.GET, '/session/:sessionId/orientation')
  SET_SCREEN_ORIENTATION = (_Method.POST, '/session/:sessionId/orientation')
  DELETE_SCREEN_ORIENTATION = (
      _Method.DELETE, '/session/:sessionId/orientation')
  MOUSE_CLICK = (_Method.POST, '/session/:sessionId/click')
  MOUSE_DOUBLE_CLICK = (_Method.POST, '/session/:sessionId/doubleclick')
  MOUSE_BUTTON_DOWN = (_Method.POST, '/session/:sessionId/buttondown')
  MOUSE_BUTTON_UP = (_Method.POST, '/session/:sessionId/buttonup')
  MOUSE_MOVE_TO = (_Method.POST, '/session/:sessionId/moveto')
  SEND_KEYS_TO_ACTIVE_ELEMENT = (_Method.POST, '/session/:sessionId/keys')
  TOUCH_SINGLE_TAP = (_Method.POST, '/session/:sessionId/touch/click')
  TOUCH_DOWN = (_Method.POST, '/session/:sessionId/touch/down')
  TOUCH_UP = (_Method.POST, '/session/:sessionId/touch/up')
  TOUCH_MOVE = (_Method.POST, '/session/:sessionId/touch/move')
  TOUCH_SCROLL = (_Method.POST, '/session/:sessionId/touch/scroll')
  TOUCH_DOUBLE_TAP = (_Method.POST, '/session/:sessionId/touch/doubleclick')
  TOUCH_LONG_PRESS = (_Method.POST, '/session/:sessionId/touch/longclick')
  TOUCH_FLICK = (_Method.POST, '/session/:sessionId/touch/flick')
  GET_LOG = (_Method.POST, '/session/:sessionId/log')
  GET_AVAILABLE_LOG_TYPES = (_Method.GET, '/session/:sessionId/log/types')
  IS_AUTO_REPORTING = (_Method.GET, '/session/:sessionId/autoreport')
  SET_AUTO_REPORTING = (_Method.POST, '/session/:sessionId/autoreport')
  GET_SESSION_LOGS = (_Method.POST, '/logs')
  STATUS = (_Method.GET, '/status')
  SET_NETWORK_CONNECTION = (
      _Method.POST, '/session/:sessionId/network_connection')
  SEND_COMMAND = (
      _Method.POST, '/session/:sessionId/chromium/send_command')
  SEND_COMMAND_AND_GET_RESULT = (
      _Method.POST, '/session/:sessionId/chromium/send_command_and_get_result')
  GENERATE_TEST_REPORT = (
      _Method.POST, '/session/:sessionId/reporting/generate_test_report')

  # Custom Chrome commands.
  IS_LOADING = (_Method.GET, '/session/:sessionId/is_loading')
  TOUCH_PINCH = (_Method.POST, '/session/:sessionId/touch/pinch')


class CommandExecutor(object):
  def __init__(self, server_url):
    self._server_url = server_url
    parsed_url = urlparse(server_url)
    self._http_client = httplib.HTTPConnection(
        parsed_url.hostname, parsed_url.port, timeout=30)

  def Execute(self, command, params):
    url_parts = command[1].split('/')
    substituted_parts = []
    for part in url_parts:
      if part.startswith(':'):
        key = part[1:]
        substituted_parts += [params[key]]
        del params[key]
      else:
        substituted_parts += [part]

    body = None
    if command[0] == _Method.POST:
      body = json.dumps(params)
    self._http_client.request(command[0], '/'.join(substituted_parts), body)
    response = self._http_client.getresponse()

    if response.status == 303:
      self._http_client.request(_Method.GET, response.getheader('location'))
      response = self._http_client.getresponse()
    result = json.loads(response.read())
    if response.status != 200 and 'value' not in result:
      raise RuntimeError('Server returned error: ' + response.reason)

    return result
