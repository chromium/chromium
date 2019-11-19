# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import platform
import sys
import util

import command_executor
from command_executor import Command
from webelement import WebElement
from websocket_connection import WebSocketConnection

ELEMENT_KEY_W3C = "element-6066-11e4-a52e-4f735466cecf"
ELEMENT_KEY = "ELEMENT"
MAX_RETRY_COUNT = 3

class ChromeDriverException(Exception):
  pass
class NoSuchElement(ChromeDriverException):
  pass
class NoSuchFrame(ChromeDriverException):
  pass
class UnknownCommand(ChromeDriverException):
  pass
class StaleElementReference(ChromeDriverException):
  pass
class ElementNotVisible(ChromeDriverException):
  pass
class InvalidElementState(ChromeDriverException):
  pass
class UnknownError(ChromeDriverException):
  pass
class JavaScriptError(ChromeDriverException):
  pass
class XPathLookupError(ChromeDriverException):
  pass
class Timeout(ChromeDriverException):
  pass
class NoSuchWindow(ChromeDriverException):
  pass
class InvalidCookieDomain(ChromeDriverException):
  pass
class ScriptTimeout(ChromeDriverException):
  pass
class InvalidSelector(ChromeDriverException):
  pass
class SessionNotCreated(ChromeDriverException):
  pass
class InvalidSessionId(ChromeDriverException):
  pass
class UnexpectedAlertOpen(ChromeDriverException):
  pass
class NoSuchAlert(ChromeDriverException):
  pass
class NoSuchCookie(ChromeDriverException):
  pass
class InvalidArgument(ChromeDriverException):
  pass
class ElementNotInteractable(ChromeDriverException):
  pass
class UnsupportedOperation(ChromeDriverException):
  pass

def _ExceptionForLegacyResponse(response):
  exception_class_map = {
    6: InvalidSessionId,
    7: NoSuchElement,
    8: NoSuchFrame,
    9: UnknownCommand,
    10: StaleElementReference,
    11: ElementNotVisible,
    12: InvalidElementState,
    13: UnknownError,
    17: JavaScriptError,
    19: XPathLookupError,
    21: Timeout,
    23: NoSuchWindow,
    24: InvalidCookieDomain,
    26: UnexpectedAlertOpen,
    27: NoSuchAlert,
    28: ScriptTimeout,
    32: InvalidSelector,
    33: SessionNotCreated,
    60: ElementNotInteractable,
    61: InvalidArgument,
    62: NoSuchCookie,
    405: UnsupportedOperation
  }
  status = response['status']
  msg = response['value']['message']
  return exception_class_map.get(status, ChromeDriverException)(msg)

def _ExceptionForStandardResponse(response):
  exception_map = {
    'invalid session id' : InvalidSessionId,
    'no such element': NoSuchElement,
    'no such frame': NoSuchFrame,
    'unknown command': UnknownCommand,
    'stale element reference': StaleElementReference,
    'element not interactable': ElementNotVisible,
    'invalid element state': InvalidElementState,
    'unknown error': UnknownError,
    'javascript error': JavaScriptError,
    'invalid selector': XPathLookupError,
    'timeout': Timeout,
    'no such window': NoSuchWindow,
    'invalid cookie domain': InvalidCookieDomain,
    'unexpected alert open': UnexpectedAlertOpen,
    'no such alert': NoSuchAlert,
    'script timeout': ScriptTimeout,
    'invalid selector': InvalidSelector,
    'session not created': SessionNotCreated,
    'no such cookie': NoSuchCookie,
    'invalid argument': InvalidArgument,
    'element not interactable': ElementNotInteractable,
    'unsupported operation': UnsupportedOperation,
  }

  error = response['value']['error']
  msg = response['value']['message']
  return exception_map.get(error, ChromeDriverException)(msg)

class ChromeDriver(object):
  """Starts and controls a single Chrome instance on this machine."""

  retry_count = 0
  retried_tests = []

  def __init__(self, *args, **kwargs):
    try:
      self._InternalInit(*args, **kwargs)
    except Exception as e:
      if not e.message.startswith('timed out'):
        raise
      else:
        if ChromeDriver.retry_count < MAX_RETRY_COUNT:
          ChromeDriver.retry_count = ChromeDriver.retry_count + 1
          ChromeDriver.retried_tests.append(kwargs.get('test_name'))
          self._InternalInit(*args, **kwargs)
        else:
          raise

  def _InternalInit(self, server_url, chrome_binary=None, android_package=None,
      android_activity=None, android_process=None,
      android_use_running_app=None, chrome_switches=None,
      chrome_extensions=None, chrome_log_path=None,
      debugger_address=None, logging_prefs=None,
      mobile_emulation=None, experimental_options=None,
      download_dir=None, network_connection=None,
      send_w3c_capability=True, send_w3c_request=True,
      page_load_strategy=None, unexpected_alert_behaviour=None,
      devtools_events_to_log=None, accept_insecure_certs=None,
      timeouts=None, test_name=None):
    self._executor = command_executor.CommandExecutor(server_url)
    self._server_url = server_url
    self.w3c_compliant = False
    self._websocket = None

    options = {}

    if experimental_options:
      assert isinstance(experimental_options, dict)
      options = experimental_options.copy()

    if android_package:
      options['androidPackage'] = android_package
      if android_activity:
        options['androidActivity'] = android_activity
      if android_process:
        options['androidProcess'] = android_process
      if android_use_running_app:
        options['androidUseRunningApp'] = android_use_running_app
    elif chrome_binary:
      options['binary'] = chrome_binary

    if sys.platform.startswith('linux') and android_package is None:
      if chrome_switches is None:
        chrome_switches = []
      # Workaround for crbug.com/611886.
      chrome_switches.append('no-sandbox')
      # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1695
      chrome_switches.append('disable-gpu')

    if chrome_switches is None:
      chrome_switches = []
    chrome_switches.append('force-color-profile=srgb')

    if chrome_switches:
      assert type(chrome_switches) is list
      options['args'] = chrome_switches

      # TODO(crbug.com/1011000): Work around a bug with headless on Mac.
      if util.GetPlatformName() == 'mac' and '--headless' in chrome_switches:
        options['excludeSwitches'] = ['--enable-logging']

    if mobile_emulation:
      assert type(mobile_emulation) is dict
      options['mobileEmulation'] = mobile_emulation

    if chrome_extensions:
      assert type(chrome_extensions) is list
      options['extensions'] = chrome_extensions

    if chrome_log_path:
      assert type(chrome_log_path) is str
      options['logPath'] = chrome_log_path

    if debugger_address:
      assert type(debugger_address) is str
      options['debuggerAddress'] = debugger_address

    if logging_prefs:
      assert type(logging_prefs) is dict
      log_types = ['client', 'driver', 'browser', 'server', 'performance',
        'devtools']
      log_levels = ['ALL', 'DEBUG', 'INFO', 'WARNING', 'SEVERE', 'OFF']
      for log_type, log_level in logging_prefs.iteritems():
        assert log_type in log_types
        assert log_level in log_levels
    else:
      logging_prefs = {}

    if devtools_events_to_log:
      assert type(devtools_events_to_log) is list
      options['devToolsEventsToLog'] = devtools_events_to_log

    download_prefs = {}
    if download_dir:
      if 'prefs' not in options:
        options['prefs'] = {}
      if 'download' not in options['prefs']:
        options['prefs']['download'] = {}
      options['prefs']['download']['default_directory'] = download_dir

    if send_w3c_capability is not None:
      options['w3c'] = send_w3c_capability

    params = {
        'goog:chromeOptions': options,
        'se:options': {
            'loggingPrefs': logging_prefs
        }
    }

    if page_load_strategy:
      assert type(page_load_strategy) is str
      params['pageLoadStrategy'] = page_load_strategy

    if unexpected_alert_behaviour:
      assert type(unexpected_alert_behaviour) is str
      if send_w3c_request:
        params['unhandledPromptBehavior'] = unexpected_alert_behaviour
      else:
        params['unexpectedAlertBehaviour'] = unexpected_alert_behaviour

    if network_connection:
      params['networkConnectionEnabled'] = network_connection

    if accept_insecure_certs is not None:
      params['acceptInsecureCerts'] = accept_insecure_certs

    if timeouts is not None:
      params['timeouts'] = timeouts

    if test_name is not None:
      params['goog:testName'] = test_name

    if send_w3c_request:
      params = {'capabilities': {'alwaysMatch': params}}
    else:
      params = {'desiredCapabilities': params}

    response = self._ExecuteCommand(Command.NEW_SESSION, params)
    if len(response.keys()) == 1 and 'value' in response.keys():
      self.w3c_compliant = True
      self._session_id = response['value']['sessionId']
      self.capabilities = self._UnwrapValue(response['value']['capabilities'])
    elif isinstance(response['status'], int):
      self.w3c_compliant = False
      self._session_id = response['sessionId']
      self.capabilities = self._UnwrapValue(response['value'])
    else:
      raise UnknownError("unexpected response")

  def _WrapValue(self, value):
    """Wrap value from client side for chromedriver side."""
    if isinstance(value, dict):
      converted = {}
      for key, val in value.items():
        converted[key] = self._WrapValue(val)
      return converted
    elif isinstance(value, WebElement):
      if (self.w3c_compliant):
        return {ELEMENT_KEY_W3C: value._id}
      else:
        return {ELEMENT_KEY: value._id}
    elif isinstance(value, list):
      return list(self._WrapValue(item) for item in value)
    else:
      return value

  def _UnwrapValue(self, value):
    if isinstance(value, dict):
      if (self.w3c_compliant and len(value) == 1
          and ELEMENT_KEY_W3C in value
          and isinstance(
            value[ELEMENT_KEY_W3C], basestring)):
        return WebElement(self, value[ELEMENT_KEY_W3C])
      elif (len(value) == 1 and ELEMENT_KEY in value
            and isinstance(value[ELEMENT_KEY], basestring)):
        return WebElement(self, value[ELEMENT_KEY])
      else:
        unwraped = {}
        for key, val in value.items():
          unwraped[key] = self._UnwrapValue(val)
        return unwraped
    elif isinstance(value, list):
      return list(self._UnwrapValue(item) for item in value)
    else:
      return value

  def _ExecuteCommand(self, command, params={}):
    params = self._WrapValue(params)
    response = self._executor.Execute(command, params)
    if ('status' in response
        and response['status'] != 0):
      raise _ExceptionForLegacyResponse(response)
    elif (type(response['value']) is dict
          and 'error' in response['value']):
      raise _ExceptionForStandardResponse(response)
    return response

  def ExecuteCommand(self, command, params={}):
    params['sessionId'] = self._session_id
    response = self._ExecuteCommand(command, params)
    return self._UnwrapValue(response['value'])

  def CreateWebSocketConnection(self):
    if self._websocket:
      return self._websocket
    else:
      self._websocket = WebSocketConnection(self._server_url, self._session_id)
      return self._websocket

  def GetWindowHandles(self):
    return self.ExecuteCommand(Command.GET_WINDOW_HANDLES)

  def SwitchToWindow(self, handle_or_name):
    if self.w3c_compliant:
      self.ExecuteCommand(Command.SWITCH_TO_WINDOW, {'handle': handle_or_name})
    else:
      self.ExecuteCommand(Command.SWITCH_TO_WINDOW, {'name': handle_or_name})

  def GetCurrentWindowHandle(self):
    return self.ExecuteCommand(Command.GET_CURRENT_WINDOW_HANDLE)

  def CloseWindow(self):
    return self.ExecuteCommand(Command.CLOSE)

  def Load(self, url):
    self.ExecuteCommand(Command.GET, {'url': url})

  def LaunchApp(self, app_id):
    self.ExecuteCommand(Command.LAUNCH_APP, {'id': app_id})

  def ExecuteScript(self, script, *args):
    converted_args = list(args)
    return self.ExecuteCommand(
        Command.EXECUTE_SCRIPT, {'script': script, 'args': converted_args})

  def SetPermission(self, parameters):
    return self.ExecuteCommand(Command.SET_PERMISSION, parameters)

  def ExecuteAsyncScript(self, script, *args):
    converted_args = list(args)
    return self.ExecuteCommand(
        Command.EXECUTE_ASYNC_SCRIPT,
        {'script': script, 'args': converted_args})

  def SwitchToFrame(self, id_or_name):
    if isinstance(id_or_name, basestring) and self.w3c_compliant:
        try:
          id_or_name = self.FindElement('css selector',
                                        '[id="%s"]' % id_or_name)
        except NoSuchElement:
          try:
            id_or_name = self.FindElement('css selector',
                                          '[name="%s"]' % id_or_name)
          except NoSuchElement:
            raise NoSuchFrame(id_or_name)
    self.ExecuteCommand(Command.SWITCH_TO_FRAME, {'id': id_or_name})

  def SwitchToFrameByIndex(self, index):
    self.SwitchToFrame(index)

  def SwitchToMainFrame(self):
    self.SwitchToFrame(None)

  def SwitchToParentFrame(self):
    self.ExecuteCommand(Command.SWITCH_TO_PARENT_FRAME)

  def GetSessions(self):
    return self.ExecuteCommand(Command.GET_SESSIONS)

  def GetTitle(self):
    return self.ExecuteCommand(Command.GET_TITLE)

  def GetPageSource(self):
    return self.ExecuteCommand(Command.GET_PAGE_SOURCE)

  def FindElement(self, strategy, target):
    return self.ExecuteCommand(
        Command.FIND_ELEMENT, {'using': strategy, 'value': target})

  def FindElements(self, strategy, target):
    return self.ExecuteCommand(
        Command.FIND_ELEMENTS, {'using': strategy, 'value': target})

  def GetTimeouts(self):
    return self.ExecuteCommand(Command.GET_TIMEOUTS)

  def SetTimeouts(self, params):
    return self.ExecuteCommand(Command.SET_TIMEOUTS, params)

  def GetCurrentUrl(self):
    return self.ExecuteCommand(Command.GET_CURRENT_URL)

  def GoBack(self):
    return self.ExecuteCommand(Command.GO_BACK)

  def GoForward(self):
    return self.ExecuteCommand(Command.GO_FORWARD)

  def Refresh(self):
    return self.ExecuteCommand(Command.REFRESH)

  def MouseMoveTo(self, element=None, x_offset=None, y_offset=None):
    params = {}
    if element is not None:
      params['element'] = element._id
    if x_offset is not None:
      params['xoffset'] = x_offset
    if y_offset is not None:
      params['yoffset'] = y_offset
    self.ExecuteCommand(Command.MOUSE_MOVE_TO, params)

  def MouseClick(self, button=0):
    self.ExecuteCommand(Command.MOUSE_CLICK, {'button': button})

  def MouseButtonDown(self, button=0):
    self.ExecuteCommand(Command.MOUSE_BUTTON_DOWN, {'button': button})

  def MouseButtonUp(self, button=0):
    self.ExecuteCommand(Command.MOUSE_BUTTON_UP, {'button': button})

  def MouseDoubleClick(self, button=0):
    self.ExecuteCommand(Command.MOUSE_DOUBLE_CLICK, {'button': button})

  def TouchDown(self, x, y):
    self.ExecuteCommand(Command.TOUCH_DOWN, {'x': x, 'y': y})

  def TouchUp(self, x, y):
    self.ExecuteCommand(Command.TOUCH_UP, {'x': x, 'y': y})

  def TouchMove(self, x, y):
    self.ExecuteCommand(Command.TOUCH_MOVE, {'x': x, 'y': y})

  def TouchScroll(self, element, xoffset, yoffset):
    params = {'element': element._id, 'xoffset': xoffset, 'yoffset': yoffset}
    self.ExecuteCommand(Command.TOUCH_SCROLL, params)

  def TouchFlick(self, element, xoffset, yoffset, speed):
    params = {
        'element': element._id,
        'xoffset': xoffset,
        'yoffset': yoffset,
        'speed': speed
    }
    self.ExecuteCommand(Command.TOUCH_FLICK, params)

  def PerformActions(self, actions):
    """
    actions: a dictionary containing the specified actions users wish to perform
    """
    self.ExecuteCommand(Command.PERFORM_ACTIONS, actions)

  def ReleaseActions(self):
    self.ExecuteCommand(Command.RELEASE_ACTIONS)

  def GetCookies(self):
    return self.ExecuteCommand(Command.GET_COOKIES)

  def GetNamedCookie(self, name):
    return self.ExecuteCommand(Command.GET_NAMED_COOKIE, {'name': name})

  def AddCookie(self, cookie):
    self.ExecuteCommand(Command.ADD_COOKIE, {'cookie': cookie})

  def DeleteCookie(self, name):
    self.ExecuteCommand(Command.DELETE_COOKIE, {'name': name})

  def DeleteAllCookies(self):
    self.ExecuteCommand(Command.DELETE_ALL_COOKIES)

  def IsAlertOpen(self):
    return self.ExecuteCommand(Command.GET_ALERT)

  def GetAlertMessage(self):
    return self.ExecuteCommand(Command.GET_ALERT_TEXT)

  def HandleAlert(self, accept, prompt_text=''):
    if prompt_text:
      self.ExecuteCommand(Command.SET_ALERT_VALUE, {'text': prompt_text})
    if accept:
      cmd = Command.ACCEPT_ALERT
    else:
      cmd = Command.DISMISS_ALERT
    self.ExecuteCommand(cmd)

  def IsLoading(self):
    return self.ExecuteCommand(Command.IS_LOADING)

  def GetWindowPosition(self):
    position = self.ExecuteCommand(Command.GET_WINDOW_POSITION,
                                   {'windowHandle': 'current'})
    return [position['x'], position['y']]

  def SetWindowPosition(self, x, y):
    self.ExecuteCommand(Command.SET_WINDOW_POSITION,
                        {'windowHandle': 'current', 'x': x, 'y': y})

  def GetWindowSize(self):
    size = self.ExecuteCommand(Command.GET_WINDOW_SIZE,
                               {'windowHandle': 'current'})
    return [size['width'], size['height']]

  def NewWindow(self, window_type="window"):
    return self.ExecuteCommand(Command.NEW_WINDOW,
                               {'type': window_type})

  def GetWindowRect(self):
    rect = self.ExecuteCommand(Command.GET_WINDOW_RECT)
    return [rect['width'], rect['height'], rect['x'], rect['y']]

  def SetWindowSize(self, width, height):
    return self.ExecuteCommand(
        Command.SET_WINDOW_SIZE,
        {'windowHandle': 'current', 'width': width, 'height': height})

  def SetWindowRect(self, width, height, x, y):
    return self.ExecuteCommand(
        Command.SET_WINDOW_RECT,
        {'width': width, 'height': height, 'x': x, 'y': y})

  def MaximizeWindow(self):
    return self.ExecuteCommand(Command.MAXIMIZE_WINDOW,
                               {'windowHandle': 'current'})

  def MinimizeWindow(self):
    return self.ExecuteCommand(Command.MINIMIZE_WINDOW,
                               {'windowHandle': 'current'})

  def FullScreenWindow(self):
    return self.ExecuteCommand(Command.FULLSCREEN_WINDOW)

  def TakeScreenshot(self):
    return self.ExecuteCommand(Command.SCREENSHOT)

  def Quit(self):
    """Quits the browser and ends the session."""
    self.ExecuteCommand(Command.QUIT)

  def GetLog(self, type):
    return self.ExecuteCommand(Command.GET_LOG, {'type': type})

  def GetAvailableLogTypes(self):
    return self.ExecuteCommand(Command.GET_AVAILABLE_LOG_TYPES)

  def SetNetworkConditions(self, latency, download_throughput,
                           upload_throughput, offline=False):
    # Until http://crbug.com/456324 is resolved, we'll always set 'offline' to
    # False, as going "offline" will sever Chromedriver's connection to Chrome.
    params = {
        'network_conditions': {
            'offline': offline,
            'latency': latency,
            'download_throughput': download_throughput,
            'upload_throughput': upload_throughput
        }
    }
    self.ExecuteCommand(Command.SET_NETWORK_CONDITIONS, params)

  def SetNetworkConditionsName(self, network_name):
    self.ExecuteCommand(
        Command.SET_NETWORK_CONDITIONS, {'network_name': network_name})

  def GetNetworkConditions(self):
    conditions = self.ExecuteCommand(Command.GET_NETWORK_CONDITIONS)
    return {
        'latency': conditions['latency'],
        'download_throughput': conditions['download_throughput'],
        'upload_throughput': conditions['upload_throughput'],
        'offline': conditions['offline']
    }

  def GetNetworkConnection(self):
    return self.ExecuteCommand(Command.GET_NETWORK_CONNECTION)

  def DeleteNetworkConditions(self):
    self.ExecuteCommand(Command.DELETE_NETWORK_CONDITIONS)

  def SetNetworkConnection(self, connection_type):
    params = {'parameters': {'type': connection_type}}
    return self.ExecuteCommand(Command.SET_NETWORK_CONNECTION, params)

  def SendCommandAndGetResult(self, cmd, cmd_params):
    params = {'cmd': cmd, 'params': cmd_params};
    return self.ExecuteCommand(Command.SEND_COMMAND_AND_GET_RESULT, params)

  def SendKeys(self, *values):
    typing = []
    for value in values:
      if isinstance(value, int):
        value = str(value)
      for i in range(len(value)):
        typing.append(value[i])
    self.ExecuteCommand(Command.SEND_KEYS_TO_ACTIVE_ELEMENT, {'value': typing})

  def GenerateTestReport(self, message):
    self.ExecuteCommand(Command.GENERATE_TEST_REPORT, {'message': message})

  def AddVirtualAuthenticator(self, protocol=None, transport=None,
                              hasResidentKey=None, hasUserVerification=None,
                              isUserConsenting=None, isUserVerified=None):
    options = {}
    if protocol is not None:
      options['protocol'] = protocol
    if transport is not None:
      options['transport'] = transport
    if hasResidentKey is not None:
      options['hasResidentKey'] = hasResidentKey
    if hasUserVerification is not None:
      options['hasUserVerification'] = hasUserVerification
    if isUserConsenting is not None:
      options['isUserConsenting'] = isUserConsenting
    if isUserVerified is not None:
      options['isUserVerified'] = isUserVerified

    return self.ExecuteCommand(Command.ADD_VIRTUAL_AUTHENTICATOR, options)

  def RemoveVirtualAuthenticator(self, authenticatorId):
    params = {'authenticatorId': authenticatorId}
    return self.ExecuteCommand(Command.REMOVE_VIRTUAL_AUTHENTICATOR, params)

  def AddCredential(self, authenticatorId=None, credentialId=None,
                    isResidentCredential=None, rpId=None, privateKey=None,
                    userHandle=None, signCount=None):
    options = {}
    if authenticatorId is not None:
      options['authenticatorId'] = authenticatorId
    if credentialId is not None:
      options['credentialId'] = credentialId
    if isResidentCredential is not None:
      options['isResidentCredential'] = isResidentCredential
    if rpId is not None:
      options['rpId'] = rpId
    if privateKey is not None:
      options['privateKey'] = privateKey
    if userHandle is not None:
      options['userHandle'] = userHandle
    if signCount is not None:
      options['signCount'] = signCount
    return self.ExecuteCommand(Command.ADD_CREDENTIAL, options)

  def GetCredentials(self, authenticatorId):
    params = {'authenticatorId': authenticatorId}
    return self.ExecuteCommand(Command.GET_CREDENTIALS, params)

  def RemoveCredential(self, authenticatorId, credentialId):
    params = {'authenticatorId': authenticatorId,
              'credentialId': credentialId}
    return self.ExecuteCommand(Command.REMOVE_CREDENTIAL, params)

  def RemoveAllCredentials(self, authenticatorId):
    params = {'authenticatorId': authenticatorId}
    return self.ExecuteCommand(Command.REMOVE_ALL_CREDENTIALS, params)

  def SetUserVerified(self, authenticatorId, isUserVerified):
    params = {'authenticatorId': authenticatorId,
              'isUserVerified': isUserVerified}
    return self.ExecuteCommand(Command.SET_USER_VERIFIED, params)
