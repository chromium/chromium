# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import http.client
import json
import os
import sys
from urllib.parse import urlparse

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
sys.path.insert(1, _PARENT_DIR)
import util
sys.path.remove(_PARENT_DIR)

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
  FULL_PAGE_SCREENSHOT = (_Method.GET, '/session/:sessionId/screenshot/full')
  PRINT = (_Method.POST, '/session/:sessionId/print')
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
  GET_ELEMENT_COMPUTED_LABEL = (
      _Method.GET, '/session/:sessionId/element/:id/computedlabel')
  GET_ELEMENT_COMPUTED_ROLE = (
      _Method.GET, '/session/:sessionId/element/:id/computedrole')
  GET_ELEMENT_SHADOW_ROOT = (
      _Method.GET, '/session/:sessionId/element/:id/shadow')
  FIND_ELEMENT_FROM_SHADOW_ROOT = (
      _Method.POST, '/session/:sessionId/shadow/:id/element')
  FIND_ELEMENTS_FROM_SHADOW_ROOT = (
      _Method.POST, '/session/:sessionId/shadow/:id/elements')
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
  NEW_WINDOW = (
      _Method.POST, '/session/:sessionId/window/new')
  GET_WINDOW_POSITION = (
      _Method.GET, '/session/:sessionId/window/:windowHandle/position')
  SET_WINDOW_SIZE = (
      _Method.POST, '/session/:sessionId/window/:windowHandle/size')
  SET_WINDOW_POSITION = (
      _Method.POST, '/session/:sessionId/window/:windowHandle/position')
  SET_WINDOW_RECT = (
      _Method.POST, '/session/:sessionId/window/rect')
  MAXIMIZE_WINDOW = (
      _Method.POST, '/session/:sessionId/window/maximize')
  MINIMIZE_WINDOW = (
      _Method.POST, '/session/:sessionId/window/minimize')
  FULLSCREEN_WINDOW = (
      _Method.POST, '/session/:sessionId/window/fullscreen')
  SET_DEVICE_POSTURE = (
      _Method.POST, '/session/:sessionId/deviceposture')
  CLEAR_DEVICE_POSTURE = (
      _Method.DELETE, '/session/:sessionId/deviceposture')
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
  PERFORM_ACTIONS = (_Method.POST, '/session/:sessionId/actions')
  RELEASE_ACTIONS = (_Method.DELETE, '/session/:sessionId/actions')
  GET_LOG = (_Method.POST, '/session/:sessionId/se/log')
  GET_AVAILABLE_LOG_TYPES = (_Method.GET, '/session/:sessionId/se/log/types')
  GET_SESSION_LOGS = (_Method.POST, '/logs')
  STATUS = (_Method.GET, '/status')
  SET_NETWORK_CONNECTION = (
      _Method.POST, '/session/:sessionId/network_connection')
  SEND_COMMAND_AND_GET_RESULT = (
      _Method.POST, '/session/:sessionId/chromium/send_command_and_get_result')
  GENERATE_TEST_REPORT = (
      _Method.POST, '/session/:sessionId/reporting/generate_test_report')
  SET_TIME_ZONE = (_Method.POST, '/session/:sessionId/time_zone')
  ADD_VIRTUAL_AUTHENTICATOR = (
      _Method.POST, '/session/:sessionId/webauthn/authenticator')
  REMOVE_VIRTUAL_AUTHENTICATOR = (
      _Method.DELETE,
      '/session/:sessionId/webauthn/authenticator/:authenticatorId')
  ADD_CREDENTIAL = (
      _Method.POST,
      '/session/:sessionId/webauthn/authenticator/:authenticatorId/credential')
  GET_CREDENTIALS = (
      _Method.GET,
      '/session/:sessionId/webauthn/authenticator/:authenticatorId/credentials')
  REMOVE_CREDENTIAL = (
      _Method.DELETE,
      '/session/:sessionId/webauthn/authenticator/:authenticatorId/credentials/'
      ':credentialId')
  REMOVE_ALL_CREDENTIALS = (
      _Method.DELETE,
      '/session/:sessionId/webauthn/authenticator/:authenticatorId/credentials')
  SET_USER_VERIFIED = (
      _Method.POST,
      '/session/:sessionId/webauthn/authenticator/:authenticatorId/uv')
  SET_CREDENTIAL_PROPERTIES = (
      _Method.POST,
      '/session/:sessionId/webauthn/authenticator/:authenticatorId/credentials/'
      ':credentialId/props')
  SET_SPC_TRANSACTION_MODE = (
      _Method.POST,
      '/session/:sessionId/secure-payment-confirmation/set-mode')
  SET_RPH_REGISTRATION_MODE = (
      _Method.POST,
      '/session/:sessionId/custom-handlers/set-mode')
  CREATE_VIRTUAL_SENSOR = (
      _Method.POST, '/session/:sessionId/sensor')
  UPDATE_VIRTUAL_SENSOR = (
      _Method.POST, '/session/:sessionId/sensor/:type')
  REMOVE_VIRTUAL_SENSOR = (
      _Method.DELETE, '/session/:sessionId/sensor/:type')
  GET_VIRTUAL_SENSOR_INFORMATION = (
      _Method.GET, '/session/:sessionId/sensor/:type')
  SET_PERMISSION = (
      _Method.POST, '/session/:sessionId/permissions')
  GET_CAST_SINKS = (
      _Method.GET,
      '/session/:sessionId/:vendorId/cast/get_sinks')
  CANCEL_FEDCM_DIALOG = (
      _Method.POST,
      '/session/:sessionId/fedcm/canceldialog')
  SELECT_ACCOUNT = (
      _Method.POST,
      '/session/:sessionId/fedcm/selectaccount')
  CLICK_FEDCM_DIALOG_BUTTON = (
      _Method.POST,
      '/session/:sessionId/fedcm/clickdialogbutton')
  GET_ACCOUNTS = (
      _Method.GET,
      '/session/:sessionId/fedcm/accountlist')
  GET_FEDCM_TITLE = (
      _Method.GET,
      '/session/:sessionId/fedcm/gettitle')
  GET_DIALOG_TYPE = (
      _Method.GET,
      '/session/:sessionId/fedcm/getdialogtype')
  SET_DELAY_ENABLED = (
      _Method.POST,
      '/session/:sessionId/fedcm/setdelayenabled')
  RESET_COOLDOWN = (
      _Method.POST,
      '/session/:sessionId/fedcm/resetcooldown')
  RUN_BOUNCE_TRACKING_MITIGATIONS = (
        _Method.DELETE,
        '/session/:sessionId/storage/run_bounce_tracking_mitigations')
  CREATE_VIRTUAL_PRESSURE_SOURCE = (
      _Method.POST, '/session/:sessionId/pressuresource')
  UPDATE_VIRTUAL_PRESSURE_SOURCE = (
      _Method.POST, '/session/:sessionId/pressuresource/:type')
  REMOVE_VIRTUAL_PRESSURE_SOURCE = (
      _Method.DELETE, '/session/:sessionId/pressuresource/:type')

  # Custom Chrome commands.
  IS_LOADING = (_Method.GET, '/session/:sessionId/is_loading')

class CommandExecutor(object):
  def __init__(self, server_url, http_timeout=None):
    self._server_url = server_url
    parsed_url = urlparse(server_url)
    self._http_timeout = 10
    # see https://crbug.com/1045241: short timeout seems to introduce flakiness
    if util.IsMac() or util.IsWindows():
      self._http_timeout = 60
    if http_timeout is not None:
      self._http_timeout = http_timeout
    self._http_client = http.client.HTTPConnection(
      parsed_url.hostname, parsed_url.port, timeout=self._http_timeout)

  @staticmethod
  def CreatePath(template_url_path, params):
    url_parts = template_url_path.split('/')
    substituted_parts = []
    for part in url_parts:
      if part.startswith(':'):
        key = part[1:]
        substituted_parts += [params[key]]
        del params[key]
      else:
        substituted_parts += [part]
    return '/'.join(substituted_parts)

  def HttpTimeout(self):
    return self._http_timeout

  def Execute(self, command, params):
    url_path = self.CreatePath(command[1], params)
    body = None
    if command[0] == _Method.POST:
      body = json.dumps(params)
    self._http_client.request(command[0], url_path, body)
    response = self._http_client.getresponse()

    if response.status == 303:
      self._http_client.request(_Method.GET, response.getheader('location'))
      response = self._http_client.getresponse()
    result = json.loads(response.read())
    if response.status != 200 and 'value' not in result:
      raise RuntimeError('Server returned error: ' + response.reason)

    return result
