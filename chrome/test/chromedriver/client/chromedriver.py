# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import psutil
import sys
import time
import urllib.parse
import util

import command_executor
from command_executor import Command
from webelement import WebElement
from webshadowroot import WebShadowRoot
from websocket_connection import WebSocketConnection
from exceptions import *

ELEMENT_KEY_W3C = "element-6066-11e4-a52e-4f735466cecf"
ELEMENT_KEY = "ELEMENT"
SHADOW_KEY = "shadow-6066-11e4-a52e-4f735466cecf"
MAX_RETRY_COUNT = 5

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
  error = response['value']['error']
  msg = response['value']['message']

  stacktrace = response['value']['stacktrace']
  if stacktrace:
      msg += '\n\nStackTrace:\n\n' + stacktrace

  return EXCEPTION_MAP.get(error, ChromeDriverException)(msg)

class ChromeDriver(object):
  """Starts and controls a single Chrome instance on this machine."""

  retry_count = 0
  retried_tests = []

  def __init__(self, server_url, server_pid, **kwargs):
    try:
      self._InternalInit(server_url, **kwargs)
    except Exception as e:
      if not str(e).startswith('timed out'):
        raise
      else:
        # Kill ChromeDriver child processes recursively
        # (i.e. browsers and their child processes etc)
        # when there is a timeout for launching browser
        if server_pid:
          processes = psutil.Process(server_pid).children(recursive=True)
          if len(processes):
            print('Terminating', len(processes), 'processes')
            for p in processes:
              p.terminate()

            _, alive = psutil.wait_procs(processes, timeout=3)
            if len(alive):
              print('Killing', len(alive), 'processes')
              for p in alive:
                p.kill()

        if ChromeDriver.retry_count < MAX_RETRY_COUNT:
          ChromeDriver.retried_tests.append(kwargs.get('test_name'))
          try:
            self._InternalInit(server_url, **kwargs)
          except:
            # Only count it as retry if failed
            print('Retry', ChromeDriver.retry_count, 'failed')
            ChromeDriver.retry_count = ChromeDriver.retry_count + 1
            raise
        else:
          raise

  def _InternalInit(self, server_url,
      chrome_binary=None, android_package=None,
      android_activity=None, android_process=None,
      android_use_running_app=None, chrome_switches=None,
      chrome_extensions=None, chrome_log_path=None,
      debugger_address=None, logging_prefs=None,
      mobile_emulation=None, experimental_options=None,
      download_dir=None, network_connection=None,
      send_w3c_capability=True, send_w3c_request=True,
      page_load_strategy=None, unexpected_alert_behaviour=None,
      devtools_events_to_log=None, accept_insecure_certs=None,
      timeouts=None, test_name=None, web_socket_url=None, browser_name=None,
      http_timeout=None):
    self._executor = command_executor.CommandExecutor(server_url,
                                                      http_timeout=http_timeout)
    self._server_url = server_url
    self.w3c_compliant = False
    self.debuggerAddress = None

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

    if chrome_switches is None:
      chrome_switches = []

    if sys.platform.startswith('linux') and android_package is None:
      # Workaround for crbug.com/611886.
      chrome_switches.append('no-sandbox')
      # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1695
      chrome_switches.append('disable-gpu')

    chrome_switches.append('force-color-profile=srgb')

    # Resampling can change the distance of a synthetic scroll.
    chrome_switches.append('disable-features=ResamplingScrollEvents')

    assert type(chrome_switches) is list
    options['args'] = chrome_switches

    # TODO(crbug.com/40101714): Work around a bug with headless on Mac.
    if (util.GetPlatformName() == 'mac' and
        browser_name == 'chrome-headless-shell' and
        debugger_address is None):
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
      for log_type, log_level in logging_prefs.items():
        assert log_type in log_types
        assert log_level in log_levels
    else:
      logging_prefs = {}

    if devtools_events_to_log:
      assert type(devtools_events_to_log) is list
      options['devToolsEventsToLog'] = devtools_events_to_log

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

    if web_socket_url is not None:
      params['webSocketUrl'] = web_socket_url

    if browser_name is not None:
      params['browserName'] = browser_name

    if send_w3c_request:
      params = {'capabilities': {'alwaysMatch': params}}
    else:
      params = {'desiredCapabilities': params}

    response = self._ExecuteCommand(Command.NEW_SESSION, params)
    if len(response.keys()) == 1 and 'value' in response.keys():
      self.w3c_compliant = True
      self._session_id = response['value']['sessionId']
      self.capabilities = self._UnwrapValue(response['value']['capabilities'])
      if ('goog:chromeOptions' in self.capabilities
          and 'debuggerAddress' in self.capabilities['goog:chromeOptions']):
          self.debuggerAddress = str(
              self.capabilities['goog:chromeOptions']['debuggerAddress'])
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
    elif isinstance(value, WebShadowRoot):
        return {SHADOW_KEY: value._id}
    elif isinstance(value, list):
      return list(self._WrapValue(item) for item in value)
    else:
      return value

  def _UnwrapValue(self, value):
    if isinstance(value, dict):
      if (self.w3c_compliant and len(value) == 1
          and ELEMENT_KEY_W3C in value
          and isinstance(
            value[ELEMENT_KEY_W3C], str)):
        return WebElement(self, value[ELEMENT_KEY_W3C])
      elif (len(value) == 1 and SHADOW_KEY in value
            and isinstance(value[SHADOW_KEY], str)):
        return WebShadowRoot(self, value[SHADOW_KEY])
      elif (len(value) == 1 and ELEMENT_KEY in value
            and isinstance(value[ELEMENT_KEY], str)):
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
    try:
      response = self._executor.Execute(command, params)
    except Exception as e:
      if str(e).startswith('timed out'):
        self._RequestCrash()
      raise e

    if ('status' in response
        and response['status'] != 0):
      raise _ExceptionForLegacyResponse(response)
    elif (type(response['value']) is dict
          and 'error' in response['value']):
      raise _ExceptionForStandardResponse(response)
    return response

  def _RequestCrash(self):
    # Can't issue a new command without session_id
    if not hasattr(self, '_session_id') or self._session_id == None:
      return
    tempDriver = ChromeDriver(self._server_url, None,
      debugger_address=self.debuggerAddress, test_name='_forceCrash')
    try:
      tempDriver.SendCommandAndGetResult("Page.crash", {})
      # allow time to complete writing the minidump
      time.sleep(5)
    except Exception as e:
      # In some cases, Chrome will not honor the request
      # Print the exception as it may give information on the Chrome state
      # but Page.crash will also generate exception, so filter that out
      message = str(e)
      if 'session deleted because of page crash' not in message:
        print('\n Exception from Page.crash: ' + message + '\n')
    tempDriver.Quit()

  def ExecuteCommand(self, command, params={}):
    params['sessionId'] = self._session_id
    response = self._ExecuteCommand(command, params)
    return self._UnwrapValue(response['value'])

  def CreateWebSocketConnection(self):
    return WebSocketConnection(self._server_url, self._session_id)

  def CreateWebSocketConnectionIPv6(self):
    url_components = urllib.parse.urlparse(self._server_url)
    new_url = urllib.parse.urlunparse(
        url_components._replace(
            netloc=('%s:%d' % ('[::1]', url_components.port))))
    return WebSocketConnection(new_url, self._session_id)

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

  def CreateVirtualSensor(self, sensor_type, sensor_params=None):
    params = {'type': sensor_type}
    if sensor_params is not None:
      params.update(sensor_params)
    return self.ExecuteCommand(Command.CREATE_VIRTUAL_SENSOR, params)

  def UpdateVirtualSensor(self, sensor_type, reading):
    params = {'type': sensor_type, 'reading': reading}
    return self.ExecuteCommand(Command.UPDATE_VIRTUAL_SENSOR, params)

  def RemoveVirtualSensor(self, sensor_type):
    params = {'type': sensor_type}
    return self.ExecuteCommand(Command.REMOVE_VIRTUAL_SENSOR, params)

  def GetVirtualSensorInformation(self, sensor_type):
    params = {'type': sensor_type}
    return self.ExecuteCommand(Command.GET_VIRTUAL_SENSOR_INFORMATION, params)

  def SetPermission(self, parameters):
    return self.ExecuteCommand(Command.SET_PERMISSION, parameters)

  def ExecuteAsyncScript(self, script, *args):
    converted_args = list(args)
    return self.ExecuteCommand(
        Command.EXECUTE_ASYNC_SCRIPT,
        {'script': script, 'args': converted_args})

  def SwitchToFrame(self, id_or_name):
    if isinstance(id_or_name, str) and self.w3c_compliant:
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
    if (len(params) == 0):
      return;
    sorted_params = sorted(params.items(), key=lambda x: x[1])
    max_kv = sorted_params[-1];
    # make sure that we have ms on the both sides of inequality
    if (self._executor.HttpTimeout() * 500 < max_kv[1]):
      raise ChromeDriverException(
        'Timeout "%s" for ChromeDriver exceeds 50%% of the '
            'HTTP connection timeout'
         % max_kv[0])
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

  def SetDevicePosture(self, posture):
    return self.ExecuteCommand(Command.SET_DEVICE_POSTURE, {'posture': posture})

  def ClearDevicePosture(self):
    return self.ExecuteCommand(Command.CLEAR_DEVICE_POSTURE)

  def TakeScreenshot(self):
    return self.ExecuteCommand(Command.SCREENSHOT)

  def TakeFullPageScreenshot(self):
    return self.ExecuteCommand(Command.FULL_PAGE_SCREENSHOT)

  def PrintPDF(self, params={}):
    return self.ExecuteCommand(Command.PRINT, params)

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

  def SetTimeZone(self, timeZone):
    return self.ExecuteCommand(Command.SET_TIME_ZONE, {'time_zone': timeZone})

  def AddVirtualAuthenticator(self, protocol=None, transport=None,
                              hasResidentKey=None, hasUserVerification=None,
                              isUserConsenting=None, isUserVerified=None,
                              extensions=None, defaultBackupState=None,
                              defaultBackupEligibility=None):
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
    if extensions is not None:
      options['extensions'] = extensions
    if defaultBackupState is not None:
      options['defaultBackupState'] = defaultBackupState
    if defaultBackupEligibility is not None:
      options['defaultBackupEligibility'] = defaultBackupEligibility

    return self.ExecuteCommand(Command.ADD_VIRTUAL_AUTHENTICATOR, options)

  def RemoveVirtualAuthenticator(self, authenticatorId):
    params = {'authenticatorId': authenticatorId}
    return self.ExecuteCommand(Command.REMOVE_VIRTUAL_AUTHENTICATOR, params)

  def AddCredential(self, authenticatorId=None, credentialId=None,
                    isResidentCredential=None, rpId=None, privateKey=None,
                    userHandle=None, signCount=None, largeBlob=None,
                    backupState=None, backupEligibility=None,userName=None,
                    userDisplayName=None):
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
    if largeBlob is not None:
      options['largeBlob'] = largeBlob
    if backupState is not None:
      options['backupState'] = backupState
    if backupEligibility is not None:
      options['backupEligibility'] = backupEligibility
    if userName is not None:
      options['userName'] = userName
    if userDisplayName is not None:
      options['userDisplayName'] = userDisplayName
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

  def SetCredentialProperties(self, authenticatorId, credentialId,
                              backupState=None, backupEligibility=None):
    params = {'authenticatorId': authenticatorId, 'credentialId': credentialId}
    if backupState is not None:
      params['backupState'] = backupState
    if backupEligibility is not None:
      params['backupEligibility'] = backupEligibility
    return self.ExecuteCommand(Command.SET_CREDENTIAL_PROPERTIES, params)

  def SetSPCTransactionMode(self, mode):
    params = {'mode': mode}
    return self.ExecuteCommand(Command.SET_SPC_TRANSACTION_MODE, params)

  def SetRPHRegistrationMode(self, mode):
    params = {'mode': mode}
    return self.ExecuteCommand(Command.SET_RPH_REGISTRATION_MODE, params)

  def CancelFedCmDialog(self):
    return self.ExecuteCommand(Command.CANCEL_FEDCM_DIALOG, {})

  def SelectAccount(self, index):
    params = {'accountIndex': index}
    return self.ExecuteCommand(Command.SELECT_ACCOUNT, params)

  def ClickFedCmDialogButton(self, dialogButton, index=None):
    params = {'dialogButton': dialogButton}
    if index is not None:
      params['index'] = index
    return self.ExecuteCommand(Command.CLICK_FEDCM_DIALOG_BUTTON, params)

  def GetAccounts(self):
    return self.ExecuteCommand(Command.GET_ACCOUNTS, {})

  def GetFedCmTitle(self):
    return self.ExecuteCommand(Command.GET_FEDCM_TITLE, {})

  def GetDialogType(self):
    return self.ExecuteCommand(Command.GET_DIALOG_TYPE, {})

  def SetDelayEnabled(self, enabled):
    params = {'enabled': enabled}
    return self.ExecuteCommand(Command.SET_DELAY_ENABLED, params)

  def ResetCooldown(self):
    return self.ExecuteCommand(Command.RESET_COOLDOWN, {})

  def RunBounceTrackingMitigations(self):
    return self.ExecuteCommand(Command.RUN_BOUNCE_TRACKING_MITIGATIONS, {})

  def GetSessionId(self):
    if not hasattr(self, '_session_id'):
      return None
    return self._session_id

  def GetCastSinks(self, vendorId):
    params = {'vendorId': vendorId}
    return self.ExecuteCommand(Command.GET_CAST_SINKS, params)

  def CreateVirtualPressureSource(self, type, metadata=None):
    params = {'type': type}
    if metadata is not None:
      params.update(metadata)
    return self.ExecuteCommand(Command.CREATE_VIRTUAL_PRESSURE_SOURCE, params)

  def UpdateVirtualPressureSource(self, type, sample):
    params = {'type': type, 'sample': sample}
    return self.ExecuteCommand(Command.UPDATE_VIRTUAL_PRESSURE_SOURCE, params)

  def RemoveVirtualPressureSource(self, type):
    params = {'type': type}
    return self.ExecuteCommand(Command.REMOVE_VIRTUAL_PRESSURE_SOURCE, params)

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Quit()
