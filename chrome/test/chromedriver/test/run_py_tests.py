#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""End to end tests for ChromeDriver."""

# Note that to run Android tests you must have the following line in
# .gclient (in the parent directory of src):  target_os = [ 'android' ]
# to get the appropriate adb version for ChromeDriver.
# TODO (crbug.com/857239): Remove above comment when adb version
# is updated in Devil.

import argparse
import base64
import codecs
import http.client
import imghdr
import json
import math
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.parse
import urllib.request


_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
_CLIENT_DIR = os.path.join(_PARENT_DIR, 'client')
_SERVER_DIR = os.path.join(_PARENT_DIR, 'server')
_TEST_DIR = os.path.join(_PARENT_DIR, 'test')

sys.path.insert(1, _PARENT_DIR)
import chrome_paths
import util
sys.path.remove(_PARENT_DIR)

sys.path.insert(1, _CLIENT_DIR)
import chromedriver
import websocket_connection
import webelement
import webshadowroot
from websocket_connection import WebSocketConnection
sys.path.remove(_CLIENT_DIR)

sys.path.insert(1, _SERVER_DIR)
import server
sys.path.remove(_SERVER_DIR)

sys.path.insert(1, _TEST_DIR)
import unittest_util
import webserver
sys.path.remove(_TEST_DIR)

sys.path.insert(0,os.path.join(chrome_paths.GetSrc(), 'third_party',
                               'catapult', 'third_party', 'gsutil',
                               'third_party', 'monotonic'))
from monotonic import monotonic

_TEST_DATA_DIR = os.path.join(chrome_paths.GetTestData(), 'chromedriver')

if util.IsLinux():
  sys.path.insert(0, os.path.join(chrome_paths.GetSrc(), 'third_party',
                                  'catapult', 'devil'))
  from devil.android import device_utils
  from devil.android import forwarder

  sys.path.insert(0, os.path.join(chrome_paths.GetSrc(), 'build', 'android'))
  import devil_chromium
  from pylib import constants

_ELEMENT_REF_REGEX = r'f\.[a-zA-Z0-9]+\.d\.[a-zA-Z0-9]+\.e\.\d+'

_NEGATIVE_FILTER = [
    # This test is too flaky on the bots, but seems to run perfectly fine
    # on developer workstations.
    'ChromeDriverTest.testEmulateNetworkConditionsNameSpeed',
    'ChromeDriverTest.testEmulateNetworkConditionsSpeed',
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=833
    'ChromeDriverTest.testAlertOnNewWindow',
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2532
    'ChromeDriverPageLoadTimeoutTest.testRefreshWithPageLoadTimeout',
    # Flaky https://bugs.chromium.org/p/chromium/issues/detail?id=1143940
    'ChromeDriverTest.testTakeLargeElementFullPageScreenshot',
    # crbug.com/chromedriver/4379
    'ChromeDriverTest.testClickElementInAnotherFrame',
    # crbug.com/chromedriver/4362
    'BidiTest.testSwitchWindows',
    'BidiTest.testOpenMultipleTabsInJavaScript',
    # crbug.com/chromedriver/4513
    'ChromeDriverSiteIsolation.testClickNavigateRemoteToSameRemote',
    # crbug.com/350916212
    'BidiTest.testFocusInFirstTab',
]


_OS_SPECIFIC_FILTER = {}
_OS_SPECIFIC_FILTER['win'] = [
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=299
    'ChromeLogPathCapabilityTest.testChromeLogPath',
    # https://bugs.chromium.org/p/chromium/issues/detail?id=1196363
    'ChromeDownloadDirTest.testFileDownloadAfterTab',
    'InvalidCertificateTest.testLoadsPage',
    'InvalidCertificateTest.testNavigateNewWindow',
    'ChromeDriverTest.testHeadlessWithUserDataDirStarts',
    'ChromeDriverTest.testHeadlessWithExistingUserDataDirStarts',
    'JavaScriptTests.testAllJS',
    'LaunchDesktopTest.testExistingDevToolsPortFile',
    'SessionHandlingTest.testQuitASessionMoreThanOnce',
    'SupportIPv4AndIPv6.testSupportIPv4AndIPv6',
    # Flaky on Win7 bots: crbug.com/1132559
    'ChromeDriverTest.testTakeElementScreenshotInIframe',
    # TODO(https://crbug.com/360058651): Flaky on win11.
    'ChromeDriverTest.testClickElementObscuredByScrollBar',
    'ChromeDriverTest.testClickElementAlmostObscuredByScrollBar',
]
_OS_SPECIFIC_FILTER['linux'] = [
]
_OS_SPECIFIC_FILTER['mac'] = [
    # https://bugs.chromium.org/p/chromium/issues/detail?id=1011225
    'ChromeDriverTest.testActionsMultiTouchPoint',
    # Flaky: https://crbug.com/1156576.
    'ChromeDriverTestLegacy.testContextMenuEventFired',
    # Flaky: https://crbug.com/1157533.
    'ChromeDriverTest.testShadowDomFindElement',
    # Flaky: https://crbug.com/1336871.
    'ChromeDriverTest.testTakeElementScreenshot',
    'ChromeDriverTest.testTakeElementScreenshotInIframe',
    'ChromeDriverTest.testTakeElementScreenshotPartlyVisible',
    'ChromeDriverTest.testTakeLargeElementScreenshot',
    # Flaky: https://crbug.com/333826980 (fails in 80% of cases on mac-arm64)
    # Error message: Timed out receiving message from renderer: 10.000
    'ChromeDriverTest.testTakeLargeElementViewportScreenshot',
    'ChromeDriverSiteIsolation.testCanClickOOPIF',
    # Flaky: https://crbug.com/1496826.
    'PureBidiTest.testParallelConnectionIsClosedOnSessionEnd',
    'PureBidiTest.testSessionCommandInEndedSession',
    # Flaky: https://crbug.com/1481915
    'ChromeDriverTest.testExecuteScriptStaleElement1',
    'ChromeDriverTest.testExecuteScriptStaleElement2',
    # Flaky: https://crbug.com/1486474
    'ChromeDriverTest.testFindChildElementStaleElement2',
    # Flaky: https://crbug.com/1486604
    'ChromeDriverTest.testFindChildElementsStaleElement2',
    # Flaky: https://crbug.com/1486520
    'ChromeDriverTest.testClickStaleElement',
    # https://issues.chromium.org/issues/42323658
    # The following tests fail on Mac due to focus issues
    'ChromeDriverSecureContextTest.testCreateVirtualSensorWithMaximumFrequency',
    'ChromeDriverSecureContextTest.testCreateVirtualSensorWithMinimumFrequency',
    'ChromeDriverSecureContextTest.testGetVirtualSensorInformation',
    'ChromeDriverSecureContextTest.testUpdateVirtualSensor',
]

_BROWSER_SPECIFIC_FILTER = {}
_BROWSER_SPECIFIC_FILTER['chrome'] = [
    # The following two tests are intended solely for chrome-headless-shell
    'ChromeDriverTest.testHeadlessWithExistingUserDataDirStarts',
    'ChromeDriverTest.testHeadlessWithUserDataDirStarts',
    # This test is a chrome-headless-shell version of testWindowFullScreen
    'ChromeDriverTest.testWindowFullScreenHeadless',
]
_BROWSER_SPECIFIC_FILTER['chrome-headless-shell'] = [
    # Maximize and FullScreeen operations make no sense in chrome-headless-mode.
    # The implementation just changes the window state.
    # S/A: BrowserHandler::setWindowsBounds at
    # //headelss/lib/browser/protocol/browser_handler.cc.
    'ChromeDriverTest.testWindowMaximize',
    'ChromeDriverTest.testWindowMaximizeFromFrame',
    'ChromeDriverTest.testWindowFullScreen',
    # chrome-headless-shell does not support scripted print
    'ChromeDriverTest.testCanSwitchToPrintPreviewDialog',
    # FedCM is not supported by chrome-headless-shell.
    'FedCmSpecificTest.*',
    # https://crbug.com/40279363
    # Bounce Tracking Mitigations is not supported by chrome-headless-shell.
    'NavTrackingMitigationSpecificTest.testRunBounceTrackingMitigations',
    # chrome-headless-shell stops handling some CDP commands until the page is
    # fully loaded.
    # See: https://crbug.com/chromedriver/4624
    'ChromeDriverTest.testSlowIFrame',
    # https://crbug.com/706008
    # chrome-headless-shell does not support extensions
    'ChromeExtensionsCapabilityTest.*',
    # chrome-headless-shell does not support chrome:about page
    'ChromeSwitchesCapabilityTest.testRemoteDebuggingPort',
    # chrome-headless-shell does not support chrome:about page
    'ChromeSwitchesCapabilityTest.testRemoteDebuggingPipe',
    # chrome-headless-shell stops handling some CDP commands until the page is
    # fully loaded.
    # See: https://crbug.com/chromedriver/4624
    'ChromeDriverTest.testShouldHandleNewWindowLoadingProperly',
    # chrome-headless-shell does not support Page.setRPHRegistrationMode
    'ChromeDriverTest.testSetRPHResgistrationMode',
    # The test is only intended for the headless mode of Chrome.
    'ChromeDriverTest.testBrowserNameHeadlessMode',
]

_BROWSER_AND_PLATFORM_SPECIFIC_FILTER = {
    'chrome': {},
    'chrome-headless-shell': {},
}
_BROWSER_AND_PLATFORM_SPECIFIC_FILTER['chrome-headless-shell']['mac'] = [
    # Unable to run chrome-headless-shell with logging enabled on Mac. See
    # crbug.com/1011000.
    'ChromeLogPathCapabilityTest.testChromeLogPath',
    # https://crbug.com/chromedriver/4631
    # chrome-headless-shell does not set the window rect as requested.
    'ChromeDriverTest.testWindowMinimize',
    'ChromeDriverTest.testWindowPosition',
    'ChromeDriverTest.testWindowRect',
    'ChromeDriverTest.testWindowSize',
    # https://crbug.com/chromedriver/4632
    # chrome-headless-shell ignores the selected range while inserting the text
    'ChromeDriverW3cTest.testSendKeysToElementDoesNotAppend',
    # https://issues.chromium.org/issues/42323658
    # The following tests fail on Mac due to focus issues
    'ComputePressureSpecificTest.testUpdateVirtualPressure',
    'ComputePressureSpecificTest.testRemoveVirtualPressureSourceWhileInUse',
]
_BROWSER_AND_PLATFORM_SPECIFIC_FILTER['chrome-headless-shell']['win'] = [
    # https://bugs.chromium.org/p/chromium/issues/detail?id=1196363
    'ChromeDriverTest.testWindowFullScreenHeadless',
]

_DESKTOP_NEGATIVE_FILTER = [
    # Desktop doesn't support touch (without --touch-events).
    'ChromeDriverTestLegacy.testTouchSingleTapElement',
    'ChromeDriverTest.testTouchDownMoveUpElement',
    'ChromeDriverTestLegacy.testTouchScrollElement',
    'ChromeDriverTestLegacy.testTouchDoubleTapElement',
    'ChromeDriverTestLegacy.testTouchLongPressElement',
    'ChromeDriverTest.testTouchFlickElement',
    'ChromeDriverAndroidTest.*',
]

_INTEGRATION_NEGATIVE_FILTER = [
    # The following test is flaky on Windows and Mac.
    'ChromeDownloadDirTest.testDownloadDirectoryOverridesExistingPreferences',
    # ChromeDriverLogTest tests an internal ChromeDriver feature, not needed
    # for integration test.
    'ChromeDriverLogTest.*',
    # ChromeDriverPageLoadTimeoutTest is flaky, particularly on Mac.
    'ChromeDriverPageLoadTimeoutTest.*',
    # Some trivial test cases that provide no additional value beyond what are
    # already tested by other test cases.
    'ChromeDriverTest.testGetCurrentWindowHandle',
    'ChromeDriverTest.testStartStop',
    # PerfTest takes a long time, requires extra setup, and adds little value
    # to integration testing.
    'PerfTest.*',
    # Flaky: https://crbug.com/899919
    'SessionHandlingTest.testGetSessions',
    # Flaky due to occasional timeout in starting Chrome
    'ZChromeStartRetryCountTest.testChromeStartRetryCount',
]


def _GetDesktopNegativeFilter(browser_name = 'chrome'):
  filter = _NEGATIVE_FILTER + _DESKTOP_NEGATIVE_FILTER
  os = util.GetPlatformName()
  if os in _OS_SPECIFIC_FILTER:
    filter += _OS_SPECIFIC_FILTER[os]
  if browser_name in _BROWSER_SPECIFIC_FILTER:
    filter += _BROWSER_SPECIFIC_FILTER[browser_name]
  if (browser_name in _BROWSER_AND_PLATFORM_SPECIFIC_FILTER and
      os in _BROWSER_AND_PLATFORM_SPECIFIC_FILTER[browser_name]):
    filter += _BROWSER_AND_PLATFORM_SPECIFIC_FILTER[browser_name][os]
  return filter

_ANDROID_NEGATIVE_FILTER = {}
_ANDROID_NEGATIVE_FILTER['chrome'] = (
    _NEGATIVE_FILTER + [
        # Android doesn't support switches and extensions.
        'ChromeSwitchesCapabilityTest.*',
        'ChromeExtensionsCapabilityTest.*',
        'MobileEmulationCapabilityTest.*',
        'ChromeDownloadDirTest.*',
        # https://crbug.com/274650
        'ChromeDriverTest.testCloseWindow',
        # Most window operations don't make sense on Android.
        'ChromeDriverTest.testWindowFullScreen',
        'ChromeDriverTest.testWindowPosition',
        'ChromeDriverTest.testWindowSize',
        'ChromeDriverTest.testWindowRect',
        'ChromeDriverTest.testWindowMaximize',
        'ChromeDriverTest.testWindowMaximizeFromFrame',
        'ChromeDriverTest.testWindowMinimize',
        'ChromeLogPathCapabilityTest.testChromeLogPath',
        # Connecting to running browser is not supported on Android.
        'RemoteBrowserTest.*',
        # Don't enable perf testing on Android yet.
        'PerfTest.*',
        # Android doesn't support multiple sessions on one device.
        'SessionHandlingTest.testGetSessions',
        # Android doesn't use the chrome://print dialog.
        'ChromeDriverTest.testCanSwitchToPrintPreviewDialog',
        # Chrome 44+ for Android doesn't dispatch the dblclick event
        'ChromeDriverTest.testMouseDoubleClick',
        # Page cannot be loaded from file:// URI in Android unless it
        # is stored in device.
        'ChromeDriverTest.testCanClickAlertInIframes',
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2081
        'ChromeDriverTest.testCloseWindowUsingJavascript',
        # Android doesn't support headless mode
        'ChromeDriverTest.testHeadlessWithUserDataDirStarts',
        'ChromeDriverTest.testHeadlessWithExistingUserDataDirStarts',
        # Tests of the desktop Chrome launch process.
        'LaunchDesktopTest.*',
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2737
        'ChromeDriverTest.testTakeElementScreenshot',
        'ChromeDriverTest.testTakeElementScreenshotPartlyVisible',
        'ChromeDriverTest.testTakeElementScreenshotInIframe',
        # setWindowBounds not supported on Android
        'ChromeDriverTest.testTakeLargeElementScreenshot',
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2786
        'ChromeDriverTest.testActionsTouchTap',
        'ChromeDriverTest.testTouchDownMoveUpElement',
        'ChromeDriverTest.testTouchFlickElement',
        # Android has no concept of tab or window, and will always lose focus
        # on tab creation. https://crbug.com/chromedriver/3018
        'ChromeDriverTest.testNewWindowDoesNotFocus',
        'ChromeDriverTest.testNewTabDoesNotFocus',
        # Android does not support the virtual authenticator environment.
        'ChromeDriverSecureContextTest.*',
        # Covered by Desktop tests; can't create 2 browsers in Android
        'SupportIPv4AndIPv6.testSupportIPv4AndIPv6',
        # Browser context management is not supported by Android
        'ChromeDriverTest.testClipboardPermissions',
        'ChromeDriverTest.testMidiPermissions',
        'ChromeDriverTest.testMultiplePermissions',
        'ChromeDriverTest.testNewWindowSameDomainHasSamePermissions',
        'ChromeDriverTest.testPermissionStates',
        'ChromeDriverTest.testPermissionsOpaqueOriginsThrowError',
        'ChromeDriverTest.testPermissionsSameOrigin',
        'ChromeDriverTest.testPermissionsSameOriginDoesNotAffectOthers',
        'ChromeDriverTest.testPersistentStoragePermissions',
        'ChromeDriverTest.testPushAndNotificationsPermissions',
        'ChromeDriverTest.testSensorPermissions',
        'ChromeDriverTest.testSettingPermissionDoesNotAffectOthers',
        # Android does not allow changing window size
        'JavaScriptTests.*',
        # These tests are failing on Android
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=3560
        'ChromeDriverTest.testTakeLargeElementViewportScreenshot',
        'ChromeDriverTest.testTakeLargeElementFullPageScreenshot',
        # Android does not support command line switches, which are
        # currently needed for these tests.
        'NavTrackingMitigationSpecificTest.testRunBounceTrackingMitigations'
    ]
)
_ANDROID_NEGATIVE_FILTER['chrome_stable'] = (
    _ANDROID_NEGATIVE_FILTER['chrome'] + [
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2350
        'ChromeDriverTest.testSlowIFrame',
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2503
        'ChromeDriverTest.testGetLogOnClosedWindow',
        'ChromeDriverTest.testGetWindowHandles',
        'ChromeDriverTest.testShouldHandleNewWindowLoadingProperly',
        'ChromeDriverTest.testSwitchToWindow',
        # Feature not yet supported in this version
        'ChromeDriverTest.testGenerateTestReport',
        # Pipe are supported only on Posix and Windows platforms
        'ChromeSwitchesCapabilityTest.testRemoteDebuggingPipe',
    ]
)
_ANDROID_NEGATIVE_FILTER['chrome_beta'] = (
    _ANDROID_NEGATIVE_FILTER['chrome'] + [
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2503
        'ChromeDriverTest.testGetLogOnClosedWindow',
        'ChromeDriverTest.testGetWindowHandles',
        'ChromeDriverTest.testShouldHandleNewWindowLoadingProperly',
        'ChromeDriverTest.testSwitchToWindow',
        # Feature not yet supported in this version
        'ChromeDriverTest.testGenerateTestReport',
        # Pipe are supported only on Posix and Windows platforms
        'ChromeSwitchesCapabilityTest.testRemoteDebuggingPipe',
    ]
)
_ANDROID_NEGATIVE_FILTER['chromium'] = (
    _ANDROID_NEGATIVE_FILTER['chrome'] + []
)
_ANDROID_NEGATIVE_FILTER['chromedriver_webview_shell'] = (
    _ANDROID_NEGATIVE_FILTER['chrome_stable'] + [
        # WebView doesn't support emulating network conditions.
        'ChromeDriverTest.testEmulateNetworkConditions',
        'ChromeDriverTest.testEmulateNetworkConditionsNameSpeed',
        'ChromeDriverTest.testEmulateNetworkConditionsOffline',
        'ChromeDriverTest.testEmulateNetworkConditionsSpeed',
        'ChromeDriverTest.testEmulateNetworkConditionsName',
        # WebView shell doesn't support popups or popup blocking.
        'ChromeDriverTest.testPopups',
        'ChromeDriverTest.testDontGoBackOrGoForward',
        # ChromeDriver WebView shell doesn't support multiple tabs.
        'ChromeDriverTest.testCloseWindowUsingJavascript',
        'ChromeDriverTest.testGetWindowHandles',
        'ChromeDriverTest.testSwitchToWindow',
        'ChromeDriverTest.testShouldHandleNewWindowLoadingProperly',
        'ChromeDriverTest.testGetLogOnClosedWindow',
        # The WebView shell that we test against (on KitKat) does not perform
        # cross-process navigations.
        # TODO(samuong): reenable when it does.
        'ChromeDriverPageLoadTimeoutTest.testPageLoadTimeoutCrossDomain',
        'ChromeDriverPageLoadTimeoutTest.'
            'testHistoryNavigationWithPageLoadTimeout',
        # Webview shell doesn't support Alerts.
        'ChromeDriverTest.testAlert',
        'ChromeDriverTest.testAlertOnNewWindow',
        'ChromeDesiredCapabilityTest.testUnexpectedAlertBehaviour',
        'ChromeDriverTest.testAlertHandlingOnPageUnload',
        'ChromeDriverTest.testClickElementAfterNavigation',
        'ChromeDriverTest.testGetLogOnWindowWithAlert',
        'ChromeDriverTest.testSendTextToAlert',
        'ChromeDriverTest.testUnexpectedAlertOpenExceptionMessage',
        # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2332
        'ChromeDriverTestLegacy.testTouchScrollElement',
        # Pipe are supported only on Posix and Windows platforms
        'ChromeSwitchesCapabilityTest.testRemoteDebuggingPipe',
    ]
)

def _GetChromePathList(driver_path, platform):
  path_mapping = {
    'linux': [os.path.join(driver_path, 'chrome')],
    'mac': [
      os.path.join(driver_path,
                   'Google Chrome for Testing.app',
                   'Contents', 'MacOS',
                   'Google Chrome for Testing'),
      os.path.join(driver_path, 'Google Chrome.app',
                   'Contents', 'MacOS', 'Google Chrome'),
      os.path.join(driver_path, 'Chromium.app',
                   'Contents', 'MacOS', 'Chromium')],
    'win': [os.path.join(driver_path, 'chrome.exe')]}
  if platform in path_mapping:
    return path_mapping[platform]
  return []

def _GetHeadlessShellPathList(driver_path, platform):
  posix_path_list = [
    os.path.join(driver_path, 'chrome-headless-shell'),
    os.path.join(driver_path, 'headless_shell')]
  path_mapping = {
    'linux': posix_path_list,
    'mac': posix_path_list,
    'win': [
      os.path.join(driver_path, 'chrome-headless-shell.exe'),
      os.path.join(driver_path, 'headless_shell.exe')]}
  if platform in path_mapping:
    return path_mapping[platform]
  return []

def _InferBrowserName(chrome_binary):
  components = os.path.split(chrome_binary)
  if len(components) == 0:
    return 'chrome' # default to chrome if path is not provided
  program = os.path.splitext(os.path.basename(chrome_binary))[0]
  if program in {'chrome-headless-shell', 'headless_shell'}:
    return 'chrome-headless-shell'
  else:
    return 'chrome'

def _InferBrowserPath(browser_name,
                      driver_path,
                      platform,
                      path_exists = os.path.exists):
  path_list = []
  if browser_name == 'chrome':
    path_list = _GetChromePathList(driver_path, platform)
  elif browser_name == 'chrome-headless-shell':
    path_list = _GetHeadlessShellPathList(driver_path, platform)
  for path in path_list:
    if path_exists(path):
      return (path, path_list)
  return (None, path_list)

class RunnerSelfTest(unittest.TestCase):
  def testPathListsForKnownPlatforms(self):
    driver_path = os.path.dirname(_CHROMEDRIVER_BINARY)
    self.assertGreater(len(_GetChromePathList(driver_path, 'linux')), 0)
    self.assertGreater(len(_GetChromePathList(driver_path, 'mac')), 0)
    self.assertGreater(len(_GetChromePathList(driver_path, 'win')), 0)
    self.assertGreater(len(_GetHeadlessShellPathList(driver_path, 'linux')), 0)
    self.assertGreater(len(_GetHeadlessShellPathList(driver_path, 'mac')), 0)
    self.assertGreater(len(_GetHeadlessShellPathList(driver_path, 'win')), 0)

  def testPathListsForUnknownPlatforms(self):
    driver_path = os.path.dirname(_CHROMEDRIVER_BINARY)
    self.assertIsNotNone(_GetChromePathList(driver_path, 'unknown'))
    self.assertIsNotNone(_GetHeadlessShellPathList(driver_path, 'unknown'))
    self.assertEqual(len(_GetChromePathList(driver_path, 'unknown')), 0)
    self.assertEqual(len(_GetHeadlessShellPathList(driver_path, 'unknown')), 0)
    self.assertIsNotNone(_GetChromePathList(driver_path, ''))
    self.assertIsNotNone(_GetHeadlessShellPathList(driver_path, ''))
    self.assertEqual(len(_GetChromePathList(driver_path, '')), 0)
    self.assertEqual(len(_GetHeadlessShellPathList(driver_path, '')), 0)

  def testBrowserNameInference(self):
    driver_path = os.path.dirname(_CHROMEDRIVER_BINARY)
    for platform in ['linux', 'mac', 'win']:
      for path in _GetChromePathList(driver_path, platform):
        self.assertEqual('chrome', _InferBrowserName(path))
      for path in _GetHeadlessShellPathList(driver_path, platform):
        self.assertEqual('chrome-headless-shell', _InferBrowserName(path))

  def testBrowserPathInference(self):
    is_headless_shell = lambda path: 'chrome-headless-shell' in path
    is_not_headless_shell = lambda path: 'chrome-headless-shell' not in path
    driver_path = os.path.dirname(_CHROMEDRIVER_BINARY)
    for platform in ['linux', 'mac', 'win']:
      # Successful inference
      self.assertIsNotNone(_InferBrowserPath('chrome',
                                driver_path,
                                platform,
                                             is_not_headless_shell)[0],
                           'platform=%s' % platform)
      self.assertIsNotNone(_InferBrowserPath('chrome-headless-shell',
                                               driver_path,
                                               platform,
                                             is_headless_shell)[0],
                           'platform=%s' % platform)
      # Failed inference
      (path, path_list) = _InferBrowserPath('chrome',
                                            driver_path,
                                            platform,
                                            lambda _: False)
      self.assertIsNone(path, 'platform=%s' % platform)
      self.assertGreater(len(path_list), 0, 'platform=%s' % platform)
      (path, path_list) = _InferBrowserPath('chrome-headless-shell',
                                            driver_path,
                                            platform,
                                            lambda _: False)
      self.assertIsNone(path, 'platform=%s' % platform)
      self.assertGreater(len(path_list), 0, 'platform=%s' % platform)


class ChromeDriverBaseTest(unittest.TestCase):
  """Base class for testing chromedriver functionalities."""

  def __init__(self, *args, **kwargs):
    super(ChromeDriverBaseTest, self).__init__(*args, **kwargs)
    self._drivers = []
    self._temp_dirs = []
    self.maxDiff = None

  def tearDown(self):
    for driver in self._drivers:
      try:
        driver.Quit()
      except:
        pass
    self._drivers = []
    for temp_dir in self._temp_dirs:
      # Deleting temp dir can fail if Chrome hasn't yet fully exited and still
      # has open files in there. So we ignore errors, and retry if necessary.
      shutil.rmtree(temp_dir, ignore_errors=True)
      retry = 0
      while retry < 10 and os.path.exists(temp_dir):
        time.sleep(0.1)
        shutil.rmtree(temp_dir, ignore_errors=True)
    self._temp_dirs = []

  def CreateTempDir(self):
    temp_dir = tempfile.mkdtemp()
    self._temp_dirs.append(temp_dir)
    return temp_dir

  def CreateDriver(self, server_url=None, server_pid=None,
                   download_dir=None, browser_name=None, **kwargs):
    if server_url is None:
      server_url = _CHROMEDRIVER_SERVER_URL
    if server_pid is None:
      server_pid = _CHROMEDRIVER_SERVER_PID

    if (not _ANDROID_PACKAGE_KEY and 'debugger_address' not in kwargs and
          '_MINIDUMP_PATH' in globals() and _MINIDUMP_PATH):
      # Environment required for minidump not supported on Android
      # minidumpPath will fail parsing if debugger_address is set
      if 'experimental_options' in kwargs:
        if 'minidumpPath' not in kwargs['experimental_options']:
          kwargs['experimental_options']['minidumpPath'] = _MINIDUMP_PATH
      else:
        kwargs['experimental_options'] = {'minidumpPath': _MINIDUMP_PATH}

    android_package = None
    android_activity = None
    android_process = None
    if _ANDROID_PACKAGE_KEY:
      android_package = constants.PACKAGE_INFO[_ANDROID_PACKAGE_KEY].package
      if _ANDROID_PACKAGE_KEY == 'chromedriver_webview_shell':
        android_activity = constants.PACKAGE_INFO[_ANDROID_PACKAGE_KEY].activity
        android_process = '%s:main' % android_package

    if browser_name is None:
      browser_name = _BROWSER_NAME

    if 'http_timeout' not in kwargs:
      kwargs['http_timeout'] = _HTTP_TIMEOUT

    driver = chromedriver.ChromeDriver(server_url, server_pid,
                                       chrome_binary=_CHROME_BINARY,
                                       browser_name=browser_name,
                                       android_package=android_package,
                                       android_activity=android_activity,
                                       android_process=android_process,
                                       download_dir=download_dir,
                                       test_name=self.id(),
                                       **kwargs)
    self._drivers += [driver]
    return driver

  def WaitForNewWindow(self, driver, old_handles, check_closed_windows=True):
    """Wait for at least one new window to show up in 20 seconds.

    Args:
      old_handles: Handles to all old windows before the new window is added.
      check_closed_windows: If True, assert that no windows are closed before
          the new window is added.

    Returns:
      Handle to a new window. None if timeout.
    """
    deadline = monotonic() + 20
    while monotonic() < deadline:
      handles = driver.GetWindowHandles()
      if check_closed_windows:
        self.assertTrue(set(old_handles).issubset(handles))
      new_handles = set(handles).difference(set(old_handles))
      if len(new_handles) > 0:
        return new_handles.pop()
      time.sleep(0.01)
    return None

  def WaitForCondition(self, predicate, timeout=5, timestep=0.1):
    """Wait for a condition to become true.

    Args:
      predicate: A function that returns a boolean value.
    """
    deadline = monotonic() + timeout
    while monotonic() < deadline:
      if predicate():
        return True
      time.sleep(timestep)
    return False


class ChromeDriverBaseTestWithWebServer(ChromeDriverBaseTest):

  @staticmethod
  def GlobalSetUp():
    ChromeDriverBaseTestWithWebServer._http_server = webserver.WebServer(
        chrome_paths.GetTestData())
    ChromeDriverBaseTestWithWebServer._sync_server = webserver.SyncWebServer()
    cert_path = os.path.join(chrome_paths.GetTestData(),
                             'chromedriver/invalid_ssl_cert.pem')
    ChromeDriverBaseTestWithWebServer._https_server = webserver.WebServer(
        chrome_paths.GetTestData(), cert_path)

    def respondWithUserAgentString(request):
      return {}, bytes("""
        <html>
        <body>%s</body>
        </html>""" % request.GetHeader('User-Agent'), 'utf-8')

    def respondWithUserAgentStringUseDeviceWidth(request):
      return {}, bytes("""
        <html>
        <head>
        <meta name="viewport" content="width=device-width,minimum-scale=1.0">
        </head>
        <body>%s</body>
        </html>""" % request.GetHeader('User-Agent'), 'utf-8')

    ChromeDriverBaseTestWithWebServer._http_server.SetCallbackForPath(
        '/userAgent', respondWithUserAgentString)
    ChromeDriverBaseTestWithWebServer._http_server.SetCallbackForPath(
        '/userAgentUseDeviceWidth', respondWithUserAgentStringUseDeviceWidth)

    if _ANDROID_PACKAGE_KEY:
      ChromeDriverBaseTestWithWebServer._device = (
          device_utils.DeviceUtils.HealthyDevices()[0])
      http_host_port = (
          ChromeDriverBaseTestWithWebServer._http_server._server.server_port)
      sync_host_port = (
          ChromeDriverBaseTestWithWebServer._sync_server._server.server_port)
      https_host_port = (
          ChromeDriverBaseTestWithWebServer._https_server._server.server_port)
      forwarder.Forwarder.Map(
          [(http_host_port, http_host_port), (sync_host_port, sync_host_port),
           (https_host_port, https_host_port)],
          ChromeDriverBaseTestWithWebServer._device)

  @staticmethod
  def GlobalTearDown():
    if _ANDROID_PACKAGE_KEY:
      forwarder.Forwarder.UnmapAllDevicePorts(ChromeDriverTest._device)
    ChromeDriverBaseTestWithWebServer._http_server.Shutdown()
    ChromeDriverBaseTestWithWebServer._https_server.Shutdown()

  @staticmethod
  def GetHttpUrlForFile(file_path):
    return ChromeDriverBaseTestWithWebServer._http_server.GetUrl() + file_path

  @staticmethod
  def ReplaceHostName(url, new_host_name):
    url_components = urllib.parse.urlparse(url)
    return urllib.parse.urlunparse(
        url_components._replace(
            netloc=('%s:%d' % (new_host_name, url_components.port))))


class ChromeDriverTestWithCustomCapability(ChromeDriverBaseTestWithWebServer):

  def testEagerMode(self):
    send_response = threading.Event()
    def waitAndRespond():
        send_response.wait(10)
        self._sync_server.RespondWithContent(b'#')
    thread = threading.Thread(target=waitAndRespond)

    self._http_server.SetDataForPath('/top.html',
     bytes("""
     <html><body>
     <div id='top'>
       <img src='%s'>
     </div>
     </body></html>""" % self._sync_server.GetUrl(), 'utf-8'))
    eager_driver = self.CreateDriver(page_load_strategy='eager')
    thread.start()
    start_eager = monotonic()
    eager_driver.Load(self._http_server.GetUrl() + '/top.html')
    stop_eager = monotonic()
    send_response.set()
    eager_time = stop_eager - start_eager
    self.assertTrue(eager_time < 9)
    thread.join()

  def testDoesntWaitWhenPageLoadStrategyIsNone(self):
    class HandleRequest(object):
      def __init__(self):
        self.sent_hello = threading.Event()

      def slowPage(self, request):
        self.sent_hello.wait(2)
        return {}, b"""
        <html>
        <body>hello</body>
        </html>"""

    handler = HandleRequest()
    self._http_server.SetCallbackForPath('/slow', handler.slowPage)

    driver = self.CreateDriver(page_load_strategy='none')
    self.assertEqual('none', driver.capabilities['pageLoadStrategy'])

    driver.Load(self._http_server.GetUrl() + '/chromedriver/empty.html')
    start = monotonic()
    driver.Load(self._http_server.GetUrl() + '/slow')
    self.assertLess(monotonic() - start, 2,
        'Loading `/slow` page should be fast enough')
    handler.sent_hello.set()
    self.WaitForCondition(lambda: 'hello' in driver.GetPageSource())
    self.assertTrue('hello' in driver.GetPageSource())

  def testUnsupportedPageLoadStrategyRaisesException(self):
    self.assertRaises(chromedriver.InvalidArgument,
                      self.CreateDriver, page_load_strategy='unsupported')

  def testGetUrlOnInvalidUrl(self):
    # Make sure we don't return 'chrome-error://chromewebdata/' (see
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1272).
    # Block DNS resolution for all hosts so that the navigation results
    # in a DNS lookup error.
    driver = self.CreateDriver(
        chrome_switches=['--host-resolver-rules=MAP * ^NOTFOUND'])
    self.assertRaises(chromedriver.ChromeDriverException,
                      driver.Load, 'http://invalid/')
    self.assertEqual('http://invalid/', driver.GetCurrentUrl())

  def testReturnedPlatformName(self):
    # This tests makes sure that the 'platformName' values defined in the
    # standard are respected by ChromeDriver:
    # https://www.w3.org/TR/webdriver2/#dfn-matching-capabilities
    driver = self.CreateDriver()
    current_platform = util.GetPlatformName()
    if current_platform == 'linux':
      self.assertEqual('linux', driver.capabilities['platformName'])
    elif current_platform == 'mac':
      self.assertEqual('mac', driver.capabilities['platformName'])
    elif current_platform == 'win':
      self.assertEqual('windows', driver.capabilities['platformName'])
    else:
      pass

class ChromeDriverWebSocketTest(ChromeDriverBaseTestWithWebServer):
  @staticmethod
  def composeWebSocketUrl(server_url, session_id):
    return server_url.replace('http', 'ws') + '/session/' + session_id

  def testDefaultSession(self):
    driver = self.CreateDriver()
    self.assertFalse('webSocketUrl' in driver.capabilities)
    self.assertRaises(Exception, websocket_connection.WebSocketConnection,
                      _CHROMEDRIVER_SERVER_URL, driver.GetSessionId())

  def testWebSocketUrlFalse(self):
    driver = self.CreateDriver(web_socket_url=False)
    self.assertFalse('webSocketUrl' in driver.capabilities)
    self.assertRaises(Exception, websocket_connection.WebSocketConnection,
                      _CHROMEDRIVER_SERVER_URL, driver.GetSessionId())

  def testWebSocketUrlTrue(self):
    driver = self.CreateDriver(web_socket_url=True)
    self.assertTrue('webSocketUrl' in driver.capabilities)
    self.assertNotEqual(None, driver.GetSessionId())
    self.assertEqual(driver.capabilities['webSocketUrl'],
        self.composeWebSocketUrl(_CHROMEDRIVER_SERVER_URL,
                                 driver.GetSessionId()))

    websocket = websocket_connection.WebSocketConnection(
        _CHROMEDRIVER_SERVER_URL, driver.GetSessionId())
    self.assertIsNotNone(websocket)

  def testWebSocketUrlInvalid(self):
    self.assertRaises(chromedriver.InvalidArgument,
        self.CreateDriver, web_socket_url='Invalid')

  def testWebSocketInvalidSessionId(self):
    self.CreateDriver(web_socket_url=True)
    self.assertRaises(Exception, websocket_connection.WebSocketConnection,
                      _CHROMEDRIVER_SERVER_URL, 'random_session_id_123')

  def testWebSocketClosedCanReconnect(self):
    driver = self.CreateDriver(web_socket_url=True)
    websocket = websocket_connection.WebSocketConnection(
        _CHROMEDRIVER_SERVER_URL, driver.GetSessionId())
    self.assertNotEqual(None, websocket)
    websocket.Close()
    websocket2 = websocket_connection.WebSocketConnection(
        _CHROMEDRIVER_SERVER_URL, driver.GetSessionId())
    self.assertNotEqual(None, websocket2)

class ChromeDriverTest(ChromeDriverBaseTestWithWebServer):
  """End to end tests for ChromeDriver."""

  def setUp(self):
    self._driver = self.CreateDriver()

  def testStartStop(self):
    pass

  def testGetComputedAttributes(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/accessibility.html'))

    firstHeaderElement = self._driver.FindElement(
      'css selector', '#first-header')

    self.assertEqual(firstHeaderElement.GetComputedLabel(), 'header content')
    self.assertEqual(firstHeaderElement.GetComputedRole(), 'heading')

  def testGetComputedAttributesForIgnoredNode(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/accessibility.html'))

    ignoredHeaderElement = self._driver.FindElement(
      'css selector', '#ignored-header')

    # GetComputedLabel for ignored node should return empty string.
    self.assertEqual(ignoredHeaderElement.GetComputedLabel(), '')
    self.assertEqual(ignoredHeaderElement.GetComputedRole(), 'none')

  def testGetComputedAttributesForUnrenderedNode(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/accessibility.html'))

    unrenderedHeaderElement = self._driver.FindElement(
      'css selector', '#unrendered-header')

    # GetComputedLabel for unrendered node should return empty string.
    self.assertEqual(unrenderedHeaderElement.GetComputedLabel(), '')
    self.assertEqual(unrenderedHeaderElement.GetComputedRole(), 'none')

  def testLoadUrl(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))

  def testGetCurrentWindowHandle(self):
    self._driver.GetCurrentWindowHandle()

  # crbug.com/p/chromedriver/issues/detail?id=2995 exposed that some libraries
  # introduce circular function references. Functions should not be serialized
  # or treated as an object - this test checks that circular function
  # definitions are allowed (despite how they are not spec-compliant.
  def testExecuteScriptWithSameFunctionReference(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript("""function copyMe() { return 1; }
                               Function.prototype.foo = copyMe;
                               const obj = {};
                               obj['buzz'] = copyMe;
                               return obj;""")

  def testFocusInFirstTab(self):
    # According to the standard the WebDriver implementation should prefer a
    # top-level browsing context (window / tab) that has system focus.
    # S/A: https://w3c.github.io/webdriver/#dfn-new-sessions
    self.assertTrue(self._driver.ExecuteScript("return document.hasFocus()"))

  def _newWindowDoesNotFocus(self, window_type='window'):
    current_handles = self._driver.GetWindowHandles()
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/focus_blur_test.html'))
    new_window = self._driver.NewWindow(window_type=window_type)
    text = self._driver.FindElement('css selector', '#result').GetText()

    self.assertTrue(new_window['handle'] not in current_handles)
    self.assertTrue(new_window['handle'] in self._driver.GetWindowHandles())
    self.assertEqual(text, 'PASS')

  def testNewWindowDoesNotFocus(self):
    self._newWindowDoesNotFocus(window_type='window')

  def testNewTabDoesNotFocus(self):
    self._newWindowDoesNotFocus(window_type='tab')

  def testCloseWindow(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('css selector', '#link').Click()
    new_window_handle = self.WaitForNewWindow(self._driver, old_handles)
    self.assertNotEqual(None, new_window_handle)
    self._driver.SwitchToWindow(new_window_handle)
    self.assertEqual(new_window_handle, self._driver.GetCurrentWindowHandle())
    self.assertRaises(chromedriver.NoSuchElement,
                      self._driver.FindElement, 'css selector', '#link')
    close_returned_handles = self._driver.CloseWindow()
    self.assertRaises(chromedriver.NoSuchWindow,
                      self._driver.GetCurrentWindowHandle)
    with self.assertRaises(chromedriver.NoSuchWindow):
      self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    with self.assertRaises(chromedriver.NoSuchWindow):
      self._driver.FindElement('css selector', '#link')
    new_handles = self._driver.GetWindowHandles()
    self.assertEqual(close_returned_handles, new_handles)
    for old_handle in old_handles:
      self.assertTrue(old_handle in new_handles)
    for handle in new_handles:
      self._driver.SwitchToWindow(handle)
      self.assertEqual(handle, self._driver.GetCurrentWindowHandle())
      close_handles = self._driver.CloseWindow()
      # CloseWindow quits the session if on the last window.
      if handle is not new_handles[-1]:
        from_get_window_handles = self._driver.GetWindowHandles()
        self.assertEqual(close_handles, from_get_window_handles)

  def testCloseWindowUsingJavascript(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('css selector', '#link').Click()
    new_window_handle = self.WaitForNewWindow(self._driver, old_handles)
    self.assertNotEqual(None, new_window_handle)
    self._driver.SwitchToWindow(new_window_handle)
    self.assertEqual(new_window_handle, self._driver.GetCurrentWindowHandle())
    self.assertRaises(chromedriver.NoSuchElement,
                      self._driver.FindElement, 'css selector', '#link')
    self._driver.ExecuteScript('window.close()')
    with self.assertRaises(chromedriver.NoSuchWindow):
      self._driver.GetTitle()
    with self.assertRaises(chromedriver.NoSuchWindow):
      self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    with self.assertRaises(chromedriver.NoSuchWindow):
      self._driver.FindElement('css selector', '#link')

  def testGetWindowHandles(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('css selector', '#link').Click()
    self.assertNotEqual(None, self.WaitForNewWindow(self._driver, old_handles))

  def testGetWindowHandlesInPresenceOfSharedWorker(self):
    self._driver.Load(
        self.GetHttpUrlForFile('/chromedriver/shared_worker.html'))
    self._driver.GetWindowHandles()

  def testSetRPHResgistrationMode(self):
    self._driver.Load(
        self.GetHttpUrlForFile('/chromedirver/page_test.html'))

    # The command expect no results if succeeded.
    result = self._driver.SetRPHRegistrationMode('autoAccept')
    self.assertEqual({}, result)

  def testSwitchToWindow(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    self.assertEqual(
        1, self._driver.ExecuteScript('window.name = "oldWindow"; return 1;'))
    window1_handle = self._driver.GetCurrentWindowHandle()
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('css selector', '#link').Click()
    new_window_handle = self.WaitForNewWindow(self._driver, old_handles)
    self.assertNotEqual(None, new_window_handle)
    self._driver.SwitchToWindow(new_window_handle)
    self.assertEqual(new_window_handle, self._driver.GetCurrentWindowHandle())
    self.assertRaises(chromedriver.NoSuchElement,
                      self._driver.FindElement, 'css selector', '#link')
    self._driver.SwitchToWindow('oldWindow')
    self.assertEqual(window1_handle, self._driver.GetCurrentWindowHandle())

  def testEvaluateScript(self):
    self.assertEqual(1, self._driver.ExecuteScript('return 1'))
    self.assertEqual(None, self._driver.ExecuteScript(''))
    self.assertEqual(None, self._driver.ExecuteScript('return null'))
    self.assertEqual(None, self._driver.ExecuteScript('return undefined'))
    self.assertEqual("Chrome", self._driver.ExecuteScript('return "Chrome"'))
    self.assertEqual([], self._driver.ExecuteScript('return []'))
    self.assertEqual([5, 6.1, "WebDriver"],
                     self._driver.ExecuteScript('return [5, 6.1, "WebDriver"]'))
    self.assertEqual({}, self._driver.ExecuteScript('return {}'))
    self.assertEqual({'one': 2, 'two': [3]},
                     self._driver.ExecuteScript(
                         'return {"one": 2, "two": [3]}'))
    self.assertEqual({},
                     self._driver.ExecuteScript(
                         'return new Map([["one", 2], ["two", [3]]])'))
    self.assertEqual({},
                     self._driver.ExecuteScript(
                         'return new Set(["one", 2])'))
    [body1, body2] = self._driver.ExecuteScript(
        'return [document.body, document.body]')
    self.assertEqual(body1._id, body2._id)
    [body2, obj1] = self._driver.ExecuteScript(
        'return [document.body, {"a": document.body}]')
    self.assertEqual(body1._id, body2._id)
    self.assertEqual(body1._id, obj1['a']._id)
    [root1, body2] = self._driver.ExecuteScript(
        'return [document.documentElement, document.body]')
    self.assertEqual(body1._id, body2._id)
    self.assertNotEqual(root1._id, body1._id)
    [root2, obj1] = self._driver.ExecuteScript(
        ''.join(['return [document.documentElement, {',
                 '  "a": document.body, ',
                 '  "b": {"c": document.documentElement}}]']))
    self.assertEqual(root1._id, root2._id)
    self.assertEqual(body1._id, obj1['a']._id)
    self.assertEqual(root1._id, obj1['b']['c']._id)


  def testEvaluateScriptWithArgs(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    script = ('document.body.innerHTML = "<div>b</div><div>c</div>";'
              'return {stuff: document.querySelectorAll("div")};')
    stuff = self._driver.ExecuteScript(script)['stuff']
    script = 'return arguments[0].innerHTML + arguments[1].innerHTML'
    self.assertEqual(
        'bc', self._driver.ExecuteScript(script, stuff[0], stuff[1]))

  def testEvaluateInvalidScript(self):
    self.assertRaises(chromedriver.ChromeDriverException,
                      self._driver.ExecuteScript, '{{{')

  def testExecuteScriptCustomArrayToJson(self):
    """Tests that redefined global Proxy object does not affect serialization"""
    self._http_server.SetDataForPath(
        '/custom-js',
        bytes("""
        <html>
        <head>
          <script>
            Array.prototype.toJSON = function() {
              return 'cusom-array-to-json';
            };
            window.createArray = function() {
              return [1, 2, 3];
            };
          </script>
        </head>
        <body>
        </body>
        </html>""", 'utf-8'))
    self._driver.Load(self._http_server.GetUrl() + '/custom-js')
    arr = self._driver.ExecuteScript('return createArray()')
    self.assertIsInstance(arr, list)
    self.assertEqual(arr, [1, 2, 3])

  def testExecuteScriptRedefinedProxy(self):
    """Tests that redefined global Proxy object does not affect serialization"""
    self._http_server.SetDataForPath(
        '/custom-js',
        bytes("""
        <html>
        <head>
          <script>
            class CustomProxy {
              toJSON() {
                return 'custom-proxy-to-json';
              }
            };
            window.Proxy = CustomProxy;
            Array.prototype.toJSON = function() {
              return 'cusom-array-to-json';
            };
            window.createArray = function() {
              return [1, 2, 3];
            };
          </script>
        </head>
        <body>
        </body>
        </html>""", 'utf-8'))
    self._driver.Load(self._http_server.GetUrl() + '/custom-js')
    arr = self._driver.ExecuteScript('return createArray()')
    self.assertIsInstance(arr, list)
    self.assertEqual(arr, [1, 2, 3])

  def testExecuteScriptCallsToJson(self):
    """Tests that own toJSON method of an object
       is called by the serialization process"""
    script = """
      const obj = {
        toJSON() {
          return [1, 2, 3];
        }
      };
      return obj;
    """
    result = self._driver.ExecuteScript(script)
    self.assertIsInstance(result, list)
    self.assertEqual(result, [1, 2, 3])

  def testExecuteScriptToJsonDataProperty(self):
    """Tests that own toJSON method of an object
       is called by the serialization process"""
    script = """
      let obj = {};
      obj.toJSON = 'text';
      return obj;
    """
    result = self._driver.ExecuteScript(script)
    self.assertEqual(result['toJSON'], 'text')

  def testExecuteScriptWithDeletedGlobalJSON(self):
    """Tests that deleted global JSON object does not affect serialization"""
    result = self._driver.ExecuteScript('delete JSON; return 321')
    self.assertIsInstance(result, int)
    self.assertEqual(result, 321)

  def testExecuteScriptStaleElement1(self):
    # Test the standard compliance of error handling
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.ExecuteScript("arguments[0].remove();", div)
    with self.assertRaises(chromedriver.StaleElementReference):
      self._driver.ExecuteScript("return arguments[0];", div)
    with self.assertRaises(chromedriver.StaleElementReference):
      self._driver.ExecuteScript("return true;", div)

  def testExecuteScriptStaleElement2(self):
    # Test the standard compliance of error handling
    div1 = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>new</div>";')
    with self.assertRaises(chromedriver.StaleElementReference):
      self._driver.ExecuteScript("return arguments[0];", div1)
    with self.assertRaises(chromedriver.StaleElementReference):
      self._driver.ExecuteScript("return true;", div1)

  def testExecuteScriptMalformedReference(self):
    # Test the standard compliance of error handling
    foo = webelement.WebElement(self._driver, "foo")
    with self.assertRaises(chromedriver.NoSuchElement):
      self._driver.ExecuteScript("return arguments[0];", foo)
    with self.assertRaises(chromedriver.NoSuchElement):
      self._driver.ExecuteScript("return true;", foo)

  def testExecuteScriptDetachedShadowRoot(self):
    # Test the standard compliance of error handling
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self._driver.ExecuteScript("arguments[0].remove();", element)
    with self.assertRaises(chromedriver.DetachedShadowRoot):
      self._driver.ExecuteScript("return arguments[0];", shadow)
    with self.assertRaises(chromedriver.DetachedShadowRoot):
      self._driver.ExecuteScript("return true;", shadow)

  def testExecuteAsyncScriptWithResolve(self):
    self.assertEqual(
        10,
        self._driver.ExecuteAsyncScript(
            'arguments[0](10)'))
    self.assertEqual(
        'one',
        self._driver.ExecuteAsyncScript(
            'arguments[0]("one")'))
    self.assertEqual(
        0.123,
        self._driver.ExecuteAsyncScript(
            'arguments[0](0.123)'))
    self.assertEqual(
        [1, 2.2, 'three'],
        self._driver.ExecuteAsyncScript(
            'arguments[0]([1, 2.2, "three"])'))

  def testExecuteAsyncScriptNoResolve(self):
    self._driver.SetTimeouts({'script': 10})
    with self.assertRaises(chromedriver.ScriptTimeout):
      self._driver.ExecuteAsyncScript('10')
    with self.assertRaises(chromedriver.ScriptTimeout):
      self._driver.ExecuteAsyncScript('"one"')
    with self.assertRaises(chromedriver.ScriptTimeout):
      self._driver.ExecuteAsyncScript('0.123')
    with self.assertRaises(chromedriver.ScriptTimeout):
      self._driver.ExecuteAsyncScript('[1, 2.2, "three"]')

  def testExecuteAsyncScript(self):
    self._driver.SetTimeouts({'script': 3000})
    self.assertRaises(
        chromedriver.ScriptTimeout,
        self._driver.ExecuteAsyncScript,
        'var callback = arguments[0];'
        'setTimeout(function(){callback(1);}, 10000);')
    self.assertEqual(
        2,
        self._driver.ExecuteAsyncScript(
            'var callback = arguments[0];'
            'setTimeout(function(){callback(2);}, 300);'))

  def testExecuteScriptTimeout(self):
    self._driver.SetTimeouts({'script': 0})
    self.assertRaises(
        chromedriver.ScriptTimeout,
        self._driver.ExecuteScript,
            'return 2')

    # Regular script can still run afterwards.
    self._driver.SetTimeouts({'script': 1000})
    self.assertEqual(
        4,
        self._driver.ExecuteScript('return 4'))

  def testSwitchToFrame(self):
    self._driver.ExecuteScript(
        'var frame = document.createElement("iframe");'
        'frame.id="id";'
        'frame.name="name";'
        'document.body.appendChild(frame);')
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('id')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('name')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrameByIndex(0)
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame(self._driver.FindElement('tag name', 'iframe'))
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))

  def testSwitchToParentFrame(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/nested.html'))
    self.assertTrue('One' in self._driver.GetPageSource())
    self._driver.SwitchToFrameByIndex(0)
    self.assertTrue('Two' in self._driver.GetPageSource())
    self._driver.SwitchToFrameByIndex(0)
    self.assertTrue('Three' in self._driver.GetPageSource())
    self._driver.SwitchToParentFrame()
    self.assertTrue('Two' in self._driver.GetPageSource())
    self._driver.SwitchToParentFrame()
    self.assertTrue('One' in self._driver.GetPageSource())

  def testSwitchToNestedFrame(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/nested_frameset.html'))
    self._driver.SwitchToFrameByIndex(0)
    self._driver.FindElement("css selector", "#link")
    self._driver.SwitchToMainFrame()
    self._driver.SwitchToFrame('2Frame')
    self._driver.FindElement("css selector", "#l1")
    self._driver.SwitchToMainFrame()
    self._driver.SwitchToFrame('fourth_frame')
    self.assertTrue('One' in self._driver.GetPageSource())
    self._driver.SwitchToMainFrame()
    self._driver.SwitchToFrameByIndex(4)
    self._driver.FindElement("css selector", "#aa1")

  def testExecuteInRemovedFrame(self):
    self._driver.ExecuteScript(
        'var frame = document.createElement("iframe");'
        'frame.id="id";'
        'frame.name="name";'
        'document.body.appendChild(frame);'
        'window.addEventListener("message",'
        '    function(event) { document.body.removeChild(frame); });')
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))
    self._driver.SwitchToFrame('id')
    self.assertTrue(self._driver.ExecuteScript('return window.top != window'))
    self._driver.ExecuteScript('parent.postMessage("remove", "*");')
    self._driver.SwitchToMainFrame()
    self.assertTrue(self._driver.ExecuteScript('return window.top == window'))

  def testSwitchToStaleFrame(self):
    self._driver.ExecuteScript(
        'var frame = document.createElement("iframe");'
        'frame.id="id";'
        'frame.name="name";'
        'document.body.appendChild(frame);')
    element = self._driver.FindElement("css selector", "#id")
    self._driver.SwitchToFrame(element)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    with self.assertRaises(chromedriver.StaleElementReference):
      self._driver.SwitchToFrame(element)

  def testReturnFrameElement(self):
    # According to https://w3c.github.io/webdriver/#execute-script
    # Upon rejection of promise the error code must be javascript error.
    # The error message must mention that the element is stale.
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/nested.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    with self.assertRaisesRegex(chromedriver.JavaScriptError, 'stale element'):
      self._driver.ExecuteScript('return window.frameElement')

  def testGetTitle(self):
    script = 'document.title = "title"; return 1;'
    self.assertEqual(1, self._driver.ExecuteScript(script))
    self.assertEqual('title', self._driver.GetTitle())

  def testGetPageSource(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    self.assertTrue('Link to empty.html' in self._driver.GetPageSource())

  def testGetElementShadowRoot(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self.assertTrue(isinstance(shadow, webshadowroot.WebShadowRoot))

  def testGetElementShadowRootNotExists(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'div')
    with self.assertRaises(chromedriver.NoSuchShadowRoot):
      element.GetElementShadowRoot()

  def testFindElementFromShadowRoot(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self.assertTrue(isinstance(shadow, webshadowroot.WebShadowRoot))
    elementInShadow = shadow.FindElement('css selector', 'input')
    self.assertTrue(isinstance(elementInShadow, webelement.WebElement))

  def testFindElementFromShadowRootInvalidArgs(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self.assertTrue(isinstance(shadow, webshadowroot.WebShadowRoot))
    with self.assertRaises(chromedriver.InvalidArgument):
      shadow.FindElement('tag name', 'input')
    with self.assertRaises(chromedriver.InvalidArgument):
      shadow.FindElement('xpath', '//')

  def testDetachedShadowRootErrorAfterRefresh(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self._driver.Refresh()
    with self.assertRaises(chromedriver.DetachedShadowRoot):
      shadow.FindElement('css selector', 'input')

  def testDetachedShadowRootErrorAfterRemoval(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self._driver.ExecuteScript("arguments[0].remove();", element)
    with self.assertRaises(chromedriver.DetachedShadowRoot):
      shadow.FindElement('css selector', 'input')

  def testFindElementsFromShadowRoot(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self.assertTrue(isinstance(shadow, webshadowroot.WebShadowRoot))
    elementsInShadow = shadow.FindElements('css selector', 'input')
    self.assertTrue(isinstance(elementsInShadow, list))
    self.assertTrue(2, len(elementsInShadow))

  def testFindElementsFromShadowRootInvalidArgs(self):
    self._driver.Load(
      self.GetHttpUrlForFile('/chromedriver/get_element_shadow_root.html'))
    element = self._driver.FindElement('tag name', 'custom-checkbox-element')
    shadow = element.GetElementShadowRoot()
    self.assertTrue(isinstance(shadow, webshadowroot.WebShadowRoot))
    with self.assertRaises(chromedriver.InvalidArgument):
      shadow.FindElements('tag name', 'input')
    with self.assertRaises(chromedriver.InvalidArgument):
      shadow.FindElements('xpath', '//')

  def testFindElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>a</div><div>b</div>";')
    self.assertTrue(
        isinstance(self._driver.FindElement('tag name', 'div'),
                   webelement.WebElement))

  def testNoSuchElementExceptionMessage(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>a</div><div>b</div>";')
    self.assertRaisesRegex(chromedriver.NoSuchElement,
                            'no such element: Unable '
                            'to locate element: {"method":"tag name",'
                            '"selector":"divine"}',
                            self._driver.FindElement,
                            'tag name', 'divine')

  def testFindElements(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>a</div><div>b</div>";')
    divs = self._driver.FindElements('tag name', 'div')
    self.assertTrue(isinstance(divs, list))
    self.assertEqual(2, len(divs))
    for div in divs:
      self.assertTrue(isinstance(div, webelement.WebElement))

  def testFindChildElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><a></a></div>";')
    element = self._driver.FindElement('tag name', 'div')
    self.assertTrue(
        isinstance(element.FindElement('tag name', 'br'),
                   webelement.WebElement))

  def testFindChildElementStaleElement1(self):
    # Test the standard compliance of error handling
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><a></a></div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.ExecuteScript("arguments[0].remove();", div)
    with self.assertRaises(chromedriver.StaleElementReference):
      div.FindElement('tag name', 'br')

  def testFindChildElementStaleElement2(self):
    # Test the standard compliance of error handling
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><a></a></div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>new</div>";')
    with self.assertRaises(chromedriver.StaleElementReference):
      div.FindElement('tag name', 'br')

  def testFindChildElements(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><br></div>";')
    element = self._driver.FindElement('tag name', 'div')
    brs = element.FindElements('tag name', 'br')
    self.assertTrue(isinstance(brs, list))
    self.assertEqual(2, len(brs))
    for br in brs:
      self.assertTrue(isinstance(br, webelement.WebElement))

  def testFindChildElementsStaleElement1(self):
    # Test the standard compliance of error handling
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><a></a></div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.ExecuteScript("arguments[0].remove();", div)
    with self.assertRaises(chromedriver.StaleElementReference):
      div.FindElements('tag name', 'br')

  def testFindChildElementsStaleElement2(self):
    # Test the standard compliance of error handling
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div><br><br></div><div><a></a></div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>new</div>";')
    with self.assertRaises(chromedriver.StaleElementReference):
      div.FindElements('tag name', 'br')

  def testFindWithInvalidSelector(self):
    selectors = ['css selector', 'xpath'];
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>a</div><div>b</div>";')
    root = self._driver.FindElement('css selector', 'div')
    for selector in selectors:
      with self.assertRaises(chromedriver.InvalidSelector):
        self._driver.FindElement(selector, '>-?!.#&<@*')
      with self.assertRaises(chromedriver.InvalidSelector):
        self._driver.FindElements(selector, '>-?!.#&<@*')
      with self.assertRaises(chromedriver.InvalidSelector):
        root.FindElement(selector, '>-?!.#&<@*')
      with self.assertRaises(chromedriver.InvalidSelector):
        root.FindElements(selector, '>-?!.#&<@*')

  def testFindWithEmptySelector(self):
    selectors = ['css selector', 'xpath'];
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>a</div><div>b</div>";')
    root = self._driver.FindElement('css selector', 'div')
    for selector in selectors:
      with self.assertRaises(chromedriver.InvalidSelector):
        self._driver.FindElement(selector, '')
      with self.assertRaises(chromedriver.InvalidSelector):
        self._driver.FindElements(selector, '')
      with self.assertRaises(chromedriver.InvalidSelector):
        root.FindElement(selector, '>-?!.#&<@*')
      with self.assertRaises(chromedriver.InvalidSelector):
        root.FindElements(selector, '>-?!.#&<@*')

  def testClickElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("click", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    div.Click()
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testClickNonExistingElement(self):
    # Test the standard compliance of error handling
    element = webelement.WebElement(self._driver, 'foo')
    with self.assertRaises(chromedriver.NoSuchElement):
      element.Click()

  def testClickStaleElement(self):
    # Test the standard compliance of error handling
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.ExecuteScript("arguments[0].remove();", div)
    with self.assertRaises(chromedriver.StaleElementReference):
      div.Click()

  def testClickElementInSubFrame(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/frame_test.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    # Test clicking element in the sub frame.
    self.testClickElement()

  def testClickElementInAnotherFrame(self):
    # Test the standard compliance of error handling
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/frame_test.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    self._driver.SwitchToParentFrame()
    self._driver.ExecuteScript("arguments[0].remove();", frame)
    with self.assertRaises(chromedriver.NoSuchElement):
      div.Click()

  def testClickElementAfterNavigation(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/link_nav.html'))
    link = self._driver.FindElement('css selector', '#l1')
    link.Click()
    alert_button = self._driver.FindElement('css selector', '#aa1')
    alert_button.Click()
    self.assertTrue(self._driver.IsAlertOpen())

  def testClickElementJustOutsidePage(self):
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=3878
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    windowHeight = self._driver.ExecuteScript('return window.innerHeight;')
    self._driver.ExecuteScript(
        '''
        document.body.innerHTML = "<div style='height:%dpx'></div>" +
          "<a href='#' onclick='return false;' id='link'>Click me</a>";
        document.body.style.cssText = "padding:0.25px";
        ''' % (2 * windowHeight))

    link = self._driver.FindElement('css selector', '#link')
    offsetTop = link.GetProperty('offsetTop')
    targetScrollTop = offsetTop - windowHeight + 1
    self._driver.ExecuteScript('window.scrollTo(0, %d);' % (targetScrollTop))
    link.Click()

  def testClickElementHavingSmallIntersectionWithindowObscuredByScrollBar(self):
    # This is a regression test for chromedriver:3933.
    # It relies on some internal knowledge on how ExecuteClickElement is
    # implemented.
    # See also: https://bugs.chromium.org/p/chromedriver/issues/detail?id=3933
    # This is what happens if the bug exists in the code:
    # Assume:
    # bar.height = 50.5 (see the CSS from horizontal_scroller.html)
    # x = 1.5 (can be any 1.5 <= x < 2.5)
    # horizontalScrollBar.height = 15
    # p = 36.5 <- position of #link relative to the viewport, calculated and
    # scrolled to by webdriver::atoms::GET_LOCATION_IN_VIEW
    # Assign:
    # window.innerHeight = floor(bar.height + x) = 52
    # Then:
    # horizontalScrollBar.y
    #   = window.innerHeight - horizontalScrollBar.height = 37
    # clickPosition.y = p + (window.innerHeight - bar.height) / 2 = 37.25
    #
    # Condition clickPosition.y > horizontalScrollBar.y means that we are
    # clicking the area obscured by horizontal scroll bar.
    # It is worth mentioning that if x < 1.5 or x >= 2.5 then 'p' will be
    # calculated differently and the bug will not reproduce.
    testcaseUrl = self.GetHttpUrlForFile(
        '/chromedriver/horizontal_scroller.html')
    self._driver.Load(testcaseUrl)
    self._driver.SetWindowRect(640, 480, None, None)
    innerHeight = self._driver.ExecuteScript('return window.innerHeight;')
    windowDecorationHeight = 480 - innerHeight
    # The value of barHeight is 50.5
    barHeight = self._driver.FindElement(
        'css selector', '#bar').GetRect()['height']
    # as mentioned above any number 1.5 <= x < 2.5 is ok provided
    # scroll.height = 15
    x = 1.5
    windowHeight = barHeight + windowDecorationHeight + x

    self._driver.SetWindowRect(640, windowHeight, None, None)
    self._driver.Load(testcaseUrl)

    link = self._driver.FindElement('css selector', '#link')
    link.Click()

    # Click must be registered
    counter = self._driver.FindElement('css selector', '#click-counter')
    self.assertEqual(1, int(counter.GetProperty('value')))

  def testClickElementObscuredByScrollBar(self):
    testcaseUrl = self.GetHttpUrlForFile(
        '/chromedriver/horizontal_scroller.html')
    self._driver.Load(testcaseUrl)
    self._driver.SetWindowRect(640, 480, None, None)
    innerHeight = self._driver.ExecuteScript('return window.innerHeight;')
    windowDecorationHeight = 480 - innerHeight
    viewportHeight = self._driver.ExecuteScript(
        'return window.visualViewport.height;')
    scrollbarHeight = innerHeight - viewportHeight
    barHeight = self._driver.FindElement(
        'css selector', '#bar').GetRect()['height']

    # -1 is used to ensure that there is no space for link before the scroll
    # bar.
    self._driver.SetWindowRect(640, math.floor(
        barHeight + windowDecorationHeight + scrollbarHeight - 1), None, None)
    self._driver.Load(testcaseUrl)
    newInnerHeight = self._driver.ExecuteScript('return window.innerHeight;')

    link = self._driver.FindElement('css selector', '#link')
    link.Click()

    rc = self._driver.ExecuteScript(
        'return document.getElementById("link").getBoundingClientRect();')
    # As link was obscured it has to be brought into view
    self.assertLess(0, rc['y'] + rc['height'])
    self.assertLess(rc['y'], newInnerHeight - scrollbarHeight)
    # Click must be registered
    counter = self._driver.FindElement('css selector', '#click-counter')
    self.assertEqual(1, int(counter.GetProperty('value')))

  def testClickElementAlmostObscuredByScrollBar(self):
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=3933
    # This test does not reproduce chromedriver:3933.
    # However it fails if the implementation contains the bug that was
    # responsible for the issue: incorrect calculation of the intersection
    # between the element and the viewport led to scrolling where the element
    # was positioned in such a way that it could not be clicked.
    testcaseUrl = self.GetHttpUrlForFile(
        '/chromedriver/horizontal_scroller.html')
    self._driver.Load(testcaseUrl)
    self._driver.SetWindowRect(640, 480, None, None)
    innerHeight = self._driver.ExecuteScript('return window.innerHeight;')
    windowDecorationHeight = 480 - innerHeight
    viewportHeight = self._driver.ExecuteScript(
        'return window.visualViewport.height;')
    scrollbarHeight = innerHeight - viewportHeight
    barHeight = self._driver.FindElement(
        'css selector', '#bar').GetRect()['height']

    # +1 is used in order to give some space for link before the scroll bar.
    self._driver.SetWindowRect(640, math.floor(
        barHeight + windowDecorationHeight + scrollbarHeight + 1), None, None)
    self._driver.Load(testcaseUrl)

    link = self._driver.FindElement('css selector', '#link')
    rc = self._driver.ExecuteScript(
        'return document.getElementById("link").getBoundingClientRect();')
    oldY = rc['y']

    link.Click()

    rc = self._driver.ExecuteScript(
        'return document.getElementById("link").getBoundingClientRect();')
    # As link is only partially obscured it must stay in place
    self.assertEqual(oldY, rc['y'])
    # Click must be registered
    counter = self._driver.FindElement('css selector', '#click-counter')
    self.assertEqual(1, int(counter.GetProperty('value')))

  def testActionsMouseMove(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("mouseover", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    actions = ({"actions": [{
      "actions": [{"duration": 32, "type": "pause"}],
      "id": "0",
      "type": "none"
      }, {
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 10, "y": 10}],
      "parameters": {"pointerType": "mouse"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testActionsMouseClick(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("click", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 10, "y": 10},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0}],
      "parameters": {"pointerType": "mouse"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testActionsMouseDoubleClick(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("dblclick", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 10, "y": 10},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0}],
      "parameters": {"pointerType": "mouse"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testActionsMouseTripleClick(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'window.click_counts = [];'
        'div.addEventListener("click", event => {'
        '  window.click_counts.push(event.detail);'
        '});'
        'return div;')
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 10, "y": 10},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0}],
      "parameters": {"pointerType": "mouse"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    click_counts = self._driver.ExecuteScript('return window.click_counts')
    self.assertEqual(3, len(click_counts))
    self.assertEqual(1, click_counts[0])
    self.assertEqual(2, click_counts[1])
    self.assertEqual(3, click_counts[2])

  def testActionsMouseResetCountOnOtherButton(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("dblclick", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 10, "y": 10},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0},
                 {"type": "pointerDown", "button": 1},
                 {"type": "pointerUp", "button": 1}],
      "parameters": {"pointerType": "mouse"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    self.assertEqual(0, len(self._driver.FindElements('tag name', 'br')))

  def testActionsMouseResetCountOnMove(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("dblclick", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 10, "y": 10},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0},
                 {"type": "pointerMove", "x": 30, "y": 10},
                 {"type": "pointerDown", "button": 0},
                 {"type": "pointerUp", "button": 0}],
      "parameters": {"pointerType": "mouse"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    self.assertEqual(0, len(self._driver.FindElements('tag name', 'br')))

  def testActionsMouseDrag(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/drag.html'))
    target = self._driver.FindElement('css selector', '#target')

    # Move to center of target element and drag it to a new location.
    actions = ({'actions': [{
      "actions": [{"duration": 32, "type": "pause"},
                  {"duration": 32, "type": "pause"},
                  {"duration": 32, "type": "pause"}],
      "id": "0",
      "type": "none"
      }, {
      'type': 'pointer',
      'actions': [
          {'type': 'pointerMove', 'x': 100, 'y': 100},
          {'type': 'pointerDown', 'button': 0},
          {'type': 'pointerMove', 'x': 150, 'y': 175}
      ],
      'parameters': {'pointerType': 'mouse'},
      'id': 'pointer1'}]})
    time.sleep(1)
    self._driver.PerformActions(actions)
    time.sleep(1)
    rect = target.GetRect()
    self.assertAlmostEqual(100, rect['x'], delta=1)
    self.assertAlmostEqual(125, rect['y'], delta=1)

    # Without releasing mouse button, should continue the drag.
    actions = ({'actions': [{
      "actions": [{"duration": 32, "type": "pause"}],
      "id": "0",
      "type": "none"
      }, {
      'type': 'pointer',
      'actions': [
          {'type': 'pointerMove', 'x': 15, 'y': 20, 'origin': 'pointer'}
      ],
      'parameters': {'pointerType': 'mouse'},
      'id': 'pointer1'}]})
    time.sleep(1)
    self._driver.PerformActions(actions)
    time.sleep(1)
    rect = target.GetRect()
    self.assertAlmostEqual(115, rect['x'], delta=1)
    self.assertAlmostEqual(145, rect['y'], delta=1)

    # Releasing mouse button stops the drag.
    actions = ({'actions': [{
      "actions": [{"duration": 32, "type": "pause"},
                  {"duration": 32, "type": "pause"}],
      "id": "0",
      "type": "none"
      }, {
      'type': 'pointer',
      'actions': [
          {'type': 'pointerUp', 'button': 0},
          {'type': 'pointerMove', 'x': 25, 'y': 25, 'origin': 'pointer'}
      ],
      'parameters': {'pointerType': 'mouse'},
      'id': 'pointer1'}]})
    time.sleep(1)
    self._driver.PerformActions(actions)
    time.sleep(1)
    rect = target.GetRect()
    self.assertAlmostEqual(115, rect['x'], delta=1)
    self.assertAlmostEqual(145, rect['y'], delta=1)

  def testActionsWheelScroll(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "1000px";'
        'div.addEventListener("wheel", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    time.sleep(1)
    actions = ({"actions": [{
      "type":"wheel",
      "actions":[{"type": "scroll", "x": 10, "y": 10, "deltaX": 5,
                  "deltaY": 15}],
      "id": "wheel1"}]})
    time.sleep(1)
    self._driver.PerformActions(actions)
    time.sleep(1)
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testActionsTouchTap(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("click", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 10, "y": 10},
                 {"type": "pointerDown"},
                 {"type": "pointerUp"}],
      "parameters": {"pointerType": "touch"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testActionsMultiTouchPoint(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        '''
        document.body.innerHTML
          = "<div id='div' autofocus style='width:200px; height:200px'>";
        window.events = [];
        const div = document.getElementById('div');
        div.addEventListener('touchstart', event => {
          window.events.push(
              {type: event.type,
               x: event.touches[event.touches.length - 1].clientX,
               y: event.touches[event.touches.length - 1].clientY});
        });
        div.addEventListener('touchend', event => {
          window.events.push(
              {type: event.type});
        });
        ''')
    time.sleep(1)

    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 50, "y": 50},
                 {"type": "pointerDown"},
                 {"type": "pointerUp"}],
      "parameters": {"pointerType": "touch"},
      "id": "pointer1"},
      {
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 60, "y": 60},
                 {"type": "pointerDown"},
                 {"type": "pointerUp"}],
      "parameters": {"pointerType": "touch"},
      "id": "pointer2"}]})
    self._driver.PerformActions(actions)
    time.sleep(1)
    events = self._driver.ExecuteScript('return window.events')
    self.assertEqual(4, len(events))
    self.assertEqual("touchstart", events[0]['type'])
    self.assertEqual("touchstart", events[1]['type'])
    self.assertEqual("touchend", events[2]['type'])
    self.assertEqual("touchend", events[3]['type'])
    self.assertAlmostEqual(50, events[0]['x'], delta=1)
    self.assertAlmostEqual(50, events[0]['y'], delta=1)
    self.assertAlmostEqual(60, events[1]['x'], delta=1)
    self.assertAlmostEqual(60, events[1]['y'], delta=1)

    self._driver.ReleaseActions()

  def testActionsMulti(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        '''
        document.body.innerHTML
          = "<div id='div' autofocus style='width:200px; height:200px'>";
        window.events = [];
        const div = document.getElementById('div');
        div.addEventListener('click', event => {
          window.events.push(
              {x: event.clientX, y: event.clientY});
        });
        ''')

    # Move mouse to (50, 50).
    self._driver.PerformActions({'actions': [
        {
            'type': 'pointer',
            'id': 'mouse',
            'actions': [ {'type': 'pointerMove', 'x': 50, 'y': 50} ]
        }
    ]})

    # Click mouse button. ChromeDriver should remember that mouse is at
    # (50, 50).
    self._driver.PerformActions({'actions': [
        {
            'type': 'pointer',
            'id': 'mouse',
            'actions': [
                {'type': 'pointerDown', "button": 0},
                {'type': 'pointerUp', "button": 0}
            ]
        }
    ]})
    events = self._driver.ExecuteScript('return window.events')
    self.assertEqual(1, len(events))
    self.assertAlmostEqual(50, events[0]['x'], delta=1)
    self.assertAlmostEqual(50, events[0]['y'], delta=1)

    # Clean up action states, move mouse back to (0, 0).
    self._driver.ReleaseActions()

    # Move mouse relative by (80, 80) pixels, and then click.
    self._driver.PerformActions({'actions': [
        {
            'type': 'pointer',
            'id': 'mouse',
            'actions': [
                {'type': 'pointerMove', 'x': 80, 'y': 80, 'origin': 'pointer'},
                {'type': 'pointerDown', "button": 0},
                {'type': 'pointerUp', "button": 0}
            ]
        }
    ]})
    events = self._driver.ExecuteScript('return window.events')
    self.assertEqual(2, len(events))
    self.assertAlmostEqual(80, events[1]['x'], delta=1)
    self.assertAlmostEqual(80, events[1]['y'], delta=1)

    self._driver.ReleaseActions()

  def testActionsPenPointerEventProperties(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        '''
        document.body.innerHTML = "<div>test</div>";
        var div = document.getElementsByTagName("div")[0];
        div.style["width"] = "100px";
        div.style["height"] = "100px";
        window.events = [];
        div.addEventListener("pointerdown", event => {
          window.events.push(
              {type: event.type,
               x: event.clientX,
               y: event.clientY,
               width: event.width,
               height: event.height,
               pressure: event.pressure,
               tiltX: event.tiltX,
               tiltY: event.tiltY,
               twist: event.twist});
        });
        ''')
    time.sleep(1)
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 30, "y": 30},
                 {"type": "pointerDown", "button": 0, "pressure":0.55,
                  "tiltX":-36, "tiltY":83, "twist":266},
                 {"type": "pointerMove", "x": 50, "y": 50},
                 {"type": "pointerUp", "button": 0}],
      "parameters": {"pointerType": "mouse"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    time.sleep(1)
    events = self._driver.ExecuteScript('return window.events')
    self.assertEqual(1, len(events))
    self.assertEqual("pointerdown", events[0]['type'])
    self.assertAlmostEqual(30, events[0]['x'], delta=1)
    self.assertAlmostEqual(30, events[0]['y'], delta=1)
    self.assertEqual(1.0, round(events[0]['width'], 2))
    self.assertEqual(1.0, round(events[0]['height'], 2))
    self.assertEqual(0.55, round(events[0]['pressure'], 2))
    self.assertEqual(-36, events[0]['tiltX'])
    self.assertEqual(83, events[0]['tiltY'])
    self.assertEqual(266, events[0]['twist'])

  def testActionsPenPointerEventPressure(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        '''
        document.body.innerHTML = "<div>test</div>";
        var div = document.getElementsByTagName("div")[0];
        div.style["width"] = "100px";
        div.style["height"] = "100px";
        window.events = [];
        var event_list = ["pointerdown", "pointermove", "pointerup"];
        for (var i = 0; i < event_list.length; i++) {
          div.addEventListener(event_list[i], event => {
            window.events.push(
                {type: event.type,
                 x: event.clientX,
                 y: event.clientY,
                 pressure: event.pressure,
                 twist: event.twist});
          });
        }
        ''')
    time.sleep(1)
    actions = ({"actions": [{
      "type":"pointer",
      "actions":[{"type": "pointerMove", "x": 30, "y": 30},
                 {"type": "pointerDown", "button": 0,
                  "twist":30},
                 {"type": "pointerMove", "x": 50, "y": 50},
                 {"type": "pointerUp", "button": 0}],
      "parameters": {"pointerType": "pen"},
      "id": "pointer1"}]})
    self._driver.PerformActions(actions)
    time.sleep(1)
    events = self._driver.ExecuteScript('return window.events')
    self.assertEqual(4, len(events))
    self.assertEqual("pointermove", events[0]['type'])
    self.assertAlmostEqual(30, events[0]['x'], delta=1)
    self.assertAlmostEqual(30, events[0]['y'], delta=1)
    self.assertEqual(0.0, round(events[0]['pressure'], 2))
    self.assertEqual(0, events[0]['twist'])
    self.assertEqual("pointerdown", events[1]['type'])
    self.assertAlmostEqual(30, events[1]['x'], delta=1)
    self.assertAlmostEqual(30, events[1]['y'], delta=1)
    self.assertEqual(0.5, round(events[1]['pressure'], 2))
    self.assertEqual(30, events[1]['twist'])
    self.assertEqual("pointermove", events[2]['type'])
    self.assertAlmostEqual(50, events[2]['x'], delta=1)
    self.assertAlmostEqual(50, events[2]['y'], delta=1)
    self.assertEqual(0.5, round(events[2]['pressure'], 2))
    self.assertEqual(0, events[2]['twist'])
    self.assertEqual("pointerup", events[3]['type'])
    self.assertAlmostEqual(50, events[3]['x'], delta=1)
    self.assertAlmostEqual(50, events[3]['y'], delta=1)
    self.assertEqual(0.0, round(events[3]['pressure'], 2))
    self.assertEqual(0, events[3]['twist'])

  def testActionsPause(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        '''
        document.body.innerHTML
          = "<input type='text' autofocus style='width:100px; height:100px'>";
        window.events = [];
        const input = document.getElementsByTagName("input")[0];
        const listener
          = e => window.events.push({type: e.type, time: e.timeStamp});
        input.addEventListener("keydown", listener);
        input.addEventListener("keyup", listener);
        input.addEventListener("mousedown", listener);
        ''')

    # Actions on 3 devices, across 6 ticks, with 200 ms pause at ticks 1 to 4.
    # Tick   "key" device   "pointer" device  "none" device
    #    0                  move
    #    1   pause 200 ms   pointer down      pause 100 ms
    #    2   "a" key down   pointer up        pause 200 ms
    #    3   "a" key up     pause 200 ms
    #    4   "b" key down   move 200 ms
    #    5   "b" key up
    actions = {'actions': [
        {
            'type': 'key',
            'id': 'key',
            'actions': [
                {'type': 'pause'},
                {'type': 'pause',    'duration': 200},
                {'type': 'keyDown',  'value': 'a'},
                {'type': 'keyUp',    'value': 'a'},
                {'type': 'keyDown',  'value': 'b'},
                {'type': 'keyUp',    'value': 'b'},
            ]
        },
        {
            'type': 'pointer',
            'id': 'mouse',
            'actions': [
                {'type': 'pointerMove',  'x': 50,  'y': 50},
                {'type': 'pointerDown',  'button': 0},
                {'type': 'pointerUp',    'button': 0},
                {'type': 'pause',        'duration': 200},
                {'type': 'pointerMove',  'duration': 200,  'x': 10,  'y': 10},
            ]
        },
        {
            'type': 'none',
            'id': 'none',
            'actions': [
                {'type': 'pause'},
                {'type': 'pause',  'duration': 100},
                {'type': 'pause',  'duration': 200},
            ]
        }
    ]}

    self._driver.PerformActions(actions)
    events = self._driver.ExecuteScript('return window.events')
    expected_events = ['mousedown', 'keydown', 'keyup', 'keydown', 'keyup']
    self.assertEqual(len(expected_events), len(events))
    for i in range(len(events)):
      self.assertEqual(expected_events[i], events[i]['type'])
      if i > 0:
        elapsed_time = events[i]['time'] - events[i-1]['time']
        self.assertGreaterEqual(elapsed_time, 200)

  def testReleaseActions(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        '''
        document.body.innerHTML
          = "<input id='target' type='text' style='width:200px; height:200px'>";
        window.events = [];
        const recordKeyEvent = event => {
          window.events.push(
              {type: event.type, code: event.code});
        };
        const recordMouseEvent = event => {
          window.events.push(
              {type: event.type, x: event.clientX, y: event.clientY});
        };
        const target = document.getElementById('target');
        target.addEventListener('keydown', recordKeyEvent);
        target.addEventListener('keyup', recordKeyEvent);
        target.addEventListener('mousedown', recordMouseEvent);
        target.addEventListener('mouseup', recordMouseEvent);
        ''')

    # Move mouse to (50, 50), press a mouse button, and press a key.
    self._driver.PerformActions({'actions': [
        {
            'type': 'pointer',
            'id': 'mouse',
            'actions': [
                {'type': 'pointerMove', 'x': 50, 'y': 50},
                {'type': 'pointerDown', "button": 0}
            ]
        },
        {
            'type': 'key',
            'id': 'key',
            'actions': [
                {'type': 'pause'},
                {'type': 'pause'},
                {'type': 'keyDown', 'value': 'a'}
            ]
        }
    ]})

    events = self._driver.ExecuteScript('return window.events')
    self.assertEqual(2, len(events))
    self.assertEqual('mousedown', events[0]['type'])
    self.assertAlmostEqual(50, events[0]['x'], delta=1)
    self.assertAlmostEqual(50, events[0]['y'], delta=1)
    self.assertEqual('keydown', events[1]['type'])
    self.assertEqual('KeyA', events[1]['code'])

    self._driver.ReleaseActions()

    events = self._driver.ExecuteScript('return window.events')
    self.assertEqual(4, len(events))
    self.assertEqual('keyup', events[2]['type'])
    self.assertEqual('KeyA', events[2]['code'])
    self.assertEqual('mouseup', events[3]['type'])
    self.assertAlmostEqual(50, events[3]['x'], delta=1)
    self.assertAlmostEqual(50, events[3]['y'], delta=1)

  def testActionsCtrlCommandKeys(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript('''
        document.write('<input type="text" id="text1" value="Hello World" />');
        document.write('<br/>')
        document.write('<input type="text" id="text2">');
        var text1 = document.getElementById("text1");
        text1.addEventListener("click", function() {
          var text1 = document.getElementById("text1");
          text1.value="new text";
        });
        ''')
    time.sleep(1)

    elem1 = self._driver.FindElement('css selector', '#text1')
    elem2 = self._driver.FindElement('css selector', '#text2')
    self.assertEqual("Hello World", elem1.GetProperty('value'))

    time.sleep(1)

    platform = util.GetPlatformName()
    modifier_key = '\uE009'
    if platform == 'mac':
      modifier_key = '\uE03D'

    # This is a sequence of actions, first move the mouse to input field
    # "elem1", then press ctrl/cmd key and 'a' key to select all the text in
    # "elem1", and then press 'x' to cut the text and move the mouse to input
    # field "elem2" and press 'v' to paste the text, and at the end, we check
    # the texts in both input fields to see if the text are cut and pasted
    # correctly from "elem1" to "elem2".
    actions = ({'actions': [{
        'type': 'key',
        'id': 'key',
        'actions': [
            {'type': 'pause'},
            {'type': 'pause'},
            {'type': 'pause'},
            {'type': 'keyDown', 'value': modifier_key},
            {'type': 'keyDown', 'value': 'a'},
            {'type': 'keyUp', 'value': 'a'},
            {'type': 'keyDown', 'value': 'x'},
            {'type': 'keyUp', 'value': 'x'},
            {'type': 'keyUp', 'value': modifier_key},
            {'type': 'pause'},
            {'type': 'pause'},
            {'type': 'pause'},
            {'type': 'keyDown', 'value': modifier_key},
            {'type': 'keyDown', 'value': 'v'},
            {'type': 'keyUp', 'value': 'v'},
            {'type': 'keyUp', 'value': modifier_key}
        ]}, {
        'type':'pointer',
        'actions':[{'type': 'pointerMove', 'x': 0, 'y': 0, 'origin': elem1},
                   {'type': 'pointerDown', 'button': 0},
                   {'type': 'pointerUp', 'button': 0},
                   {'type': 'pause'},
                   {'type': 'pause'},
                   {'type': 'pause'},
                   {'type': 'pause'},
                   {'type': 'pause'},
                   {'type': 'pause'},
                   {'type': 'pointerMove', 'x': 0, 'y': 0, 'origin': elem2},
                   {'type': 'pointerDown', 'button': 0},
                   {'type': 'pointerUp', 'button': 0},
                   {'type': 'pause'},
                   {'type': 'pause'},
                   {'type': 'pause'},
                   {'type': 'pause'}],
        'parameters': {'pointerType': 'mouse'},
        'id': 'pointer1'}
        ]})
    self._driver.PerformActions(actions)
    time.sleep(1)
    self.assertEqual("", elem1.GetProperty('value'))
    self.assertEqual("new text", elem2.GetProperty('value'))
    time.sleep(1)

  def testPageLoadStrategyIsNormalByDefault(self):
    self.assertEqual('normal',
                      self._driver.capabilities['pageLoadStrategy'])

  def testClearElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    text = self._driver.ExecuteScript(
        'document.body.innerHTML = \'<input type="text" value="abc">\';'
        'return document.getElementsByTagName("input")[0];')
    value = self._driver.ExecuteScript('return arguments[0].value;', text)
    self.assertEqual('abc', value)
    text.Clear()
    value = self._driver.ExecuteScript('return arguments[0].value;', text)
    self.assertEqual('', value)

  def testSendKeysToInputFileElement(self):
    file_name = os.path.join(_TEST_DATA_DIR, 'anchor_download_test.png')
    self._driver.Load(ChromeDriverTest.GetHttpUrlForFile(
        '/chromedriver/file_input.html'))
    elem = self._driver.FindElement('css selector', '#id_file')
    elem.SendKeys(file_name)
    text = self._driver.ExecuteScript(
        'var input = document.getElementById("id_file").value;'
        'return input;')
    self.assertEqual('C:\\fakepath\\anchor_download_test.png', text)
    if not _ANDROID_PACKAGE_KEY:
      self.assertRaises(chromedriver.InvalidArgument,
                                  elem.SendKeys, "/blah/blah/blah")

  def testSendKeysToNonTypeableInputElement(self):
    self._driver.Load("about:blank")
    self._driver.ExecuteScript(
         "document.body.innerHTML = '<input type=\"color\">';")
    elem = self._driver.FindElement('tag name', 'input')
    input_value = '#7fffd4'
    elem.SendKeys(input_value)
    value = elem.GetProperty('value')
    self.assertEqual(input_value, value)

  def testGetElementAttribute(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/attribute_colon_test.html'))
    elem = self._driver.FindElement("css selector", "*[name='phones']")
    self.assertEqual('3', elem.GetAttribute('size'))

  def testGetElementProperty(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/two_inputs.html'))
    elem = self._driver.FindElement("css selector", "#first")
    self.assertEqual('text', elem.GetProperty('type'))
    self.assertEqual('first', elem.GetProperty('id'))

  def testGetElementSpecialCharAttribute(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/attribute_colon_test.html'))
    elem = self._driver.FindElement("css selector", "*[name='phones']")
    self.assertEqual('colonvalue', elem.GetAttribute('ext:qtip'))

  def testGetCurrentUrl(self):
    url = self.GetHttpUrlForFile('/chromedriver/frame_test.html')
    self._driver.Load(url)
    self.assertEqual(url, self._driver.GetCurrentUrl())
    self._driver.SwitchToFrame(self._driver.FindElement('tag name', 'iframe'))
    self.assertEqual(url, self._driver.GetCurrentUrl())

  def testGoBackAndGoForward(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.GoBack()
    self._driver.GoForward()

  def testDontGoBackOrGoForward(self):
    # We need to run this test in a new tab so that it is isolated from previous
    # test runs.
    old_windows = self._driver.GetWindowHandles()
    self._driver.ExecuteScript('window.open("about:blank")')
    new_window = self.WaitForNewWindow(self._driver, old_windows)
    self._driver.SwitchToWindow(new_window)
    self.assertEqual('about:blank', self._driver.GetCurrentUrl())
    self._driver.GoBack()
    self.assertEqual('about:blank', self._driver.GetCurrentUrl())
    self._driver.GoForward()
    self.assertEqual('about:blank', self._driver.GetCurrentUrl())

  def testBackNavigationAfterClickElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/link_nav.html'))
    link = self._driver.FindElement('css selector', '#l1')
    link.Click()
    self._driver.GoBack()
    self.assertNotEqual('data:,', self._driver.GetCurrentUrl())
    self.assertEqual(self.GetHttpUrlForFile('/chromedriver/link_nav.html'),
                      self._driver.GetCurrentUrl())

  def testAlertHandlingOnPageUnload(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript('window.onbeforeunload=function(){return true}')
    self._driver.FindElement('tag name', 'body').Click()
    self._driver.GoBack()
    self.assertFalse(self._driver.IsAlertOpen())

  def testAlertHandlingOnNavigation(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    self._driver.ExecuteScript('window.onbeforeunload=function(){return true}')
    self._driver.FindElement('tag name', 'body').Click()
    self._driver.ExecuteAsyncScript('''
        const [url, resolve] = arguments;
        window.location.href = url;
        resolve(url);
    ''', self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self.assertFalse(self._driver.IsAlertOpen())

  def testAlertHandlingOnNavigationNoResolve(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    self._driver.ExecuteScript('window.onbeforeunload=function(){return true}')
    self._driver.FindElement('tag name', 'body').Click()
    self._driver.SetTimeouts({'script': 100})

    # The following script never calls resolve. Therefore it times out.
    with self.assertRaises(chromedriver.ScriptTimeout):
      self._driver.ExecuteAsyncScript('''
          const [url, resolve] = arguments;
          window.location.href = url;
      ''', self.GetHttpUrlForFile('/chromedriver/empty.html'))

    self.assertFalse(self._driver.IsAlertOpen())

  def testRefresh(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.Refresh()

  def testAlert(self):
    self.assertFalse(self._driver.IsAlertOpen())
    self._driver.ExecuteScript('window.confirmed = confirm(\'HI\');')
    self.assertTrue(self._driver.IsAlertOpen())
    self.assertEqual('HI', self._driver.GetAlertMessage())
    self._driver.HandleAlert(False)
    self.assertFalse(self._driver.IsAlertOpen())
    self.assertEqual(False,
                      self._driver.ExecuteScript('return window.confirmed'))

  def testSendTextToAlert(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript('prompt = window.prompt()')
    self.assertTrue(self._driver.IsAlertOpen())
    self._driver.HandleAlert(True, 'TextToPrompt')
    self.assertEqual('TextToPrompt',
                      self._driver.ExecuteScript('return prompt'))
    self._driver.ExecuteScript('window.confirmed = confirm(\'HI\');')
    self.assertRaises(chromedriver.ElementNotInteractable,
                 self._driver.HandleAlert,
                 True, 'textToConfirm')
    self._driver.HandleAlert(True) #for closing the previous alert.
    self._driver.ExecuteScript('window.onbeforeunload=function(){return true}')
    self._driver.FindElement('tag name', 'body').Click()
    self._driver.Refresh()
    self.assertFalse(self._driver.IsAlertOpen())

  def testAlertOnNewWindow(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    old_windows = self._driver.GetWindowHandles()
    self._driver.ExecuteScript("window.open('%s')" %
        self.GetHttpUrlForFile('/chromedriver/alert_onload.html'))
    new_window = self.WaitForNewWindow(self._driver, old_windows)
    self.assertNotEqual(None, new_window)
    self._driver.SwitchToWindow(new_window)
    self.assertTrue(self._driver.IsAlertOpen())
    self._driver.HandleAlert(False)
    self.assertFalse(self._driver.IsAlertOpen())

  def testShouldHandleNewWindowLoadingProperly(self):
    """Tests that ChromeDriver determines loading correctly for new windows."""
    self._http_server.SetDataForPath(
        '/newwindow',
        bytes("""
        <html>
        <body>
        <a href='%s' target='_blank'>new window/tab</a>
        </body>
        </html>""" % self._sync_server.GetUrl(), 'utf-8'))
    self._driver.Load(self._http_server.GetUrl() + '/newwindow')
    old_windows = self._driver.GetWindowHandles()
    self._driver.FindElement('tag name', 'a').Click()
    new_window = self.WaitForNewWindow(self._driver, old_windows)
    self.assertNotEqual(None, new_window)

    self.assertFalse(self._driver.IsLoading())
    self._driver.SwitchToWindow(new_window)
    self.assertTrue(self._driver.IsLoading())
    self._sync_server.RespondWithContent(b'<html>new window</html>')
    self._driver.ExecuteScript('return 1')  # Shouldn't hang.

  def testPopups(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.ExecuteScript('window.open("about:blank")')
    new_window_handle = self.WaitForNewWindow(self._driver, old_handles)
    self.assertNotEqual(None, new_window_handle)

  def testNoSuchFrame(self):
    self.assertRaises(chromedriver.NoSuchFrame,
                      self._driver.SwitchToFrame, 'nosuchframe')
    self.assertRaises(chromedriver.NoSuchFrame,
                      self._driver.SwitchToFrame,
                      self._driver.FindElement('tag name', 'body'))

    self._driver.ExecuteScript(
        'var frame = document.createElement("iframe");'
        'document.body.appendChild(frame);')
    with self.assertRaises(chromedriver.NoSuchFrame):
      self._driver.SwitchToFrameByIndex(100)

  def testWindowPosition(self):
    rect = self._driver.GetWindowRect()
    self._driver.SetWindowRect(None, None, rect[2], rect[3])
    self.assertEqual(rect, self._driver.GetWindowRect())

    # Resize so the window isn't moved offscreen.
    # See https://bugs.chromium.org/p/chromedriver/issues/detail?id=297.
    self._driver.SetWindowRect(640, 400, None, None)

    self._driver.SetWindowRect(None, None, 100, 200)
    self.assertEqual([640, 400, 100, 200], self._driver.GetWindowRect())

  def testWindowSize(self):
    rect = self._driver.GetWindowRect()
    self._driver.SetWindowRect(rect[0], rect[1], None, None)
    self.assertEqual(rect, self._driver.GetWindowRect())

    self._driver.SetWindowRect(640, 400, None, None)
    self.assertEqual([640, 400, rect[2], rect[3]],
                      self._driver.GetWindowRect())

  def testWindowRect(self):
    old_window_rect = self._driver.GetWindowRect()
    self._driver.SetWindowRect(*old_window_rect)
    self.assertEqual(self._driver.GetWindowRect(), old_window_rect)

    target_window_rect = [640, 400, 100, 200]
    target_window_rect_dict = {'width': 640, 'height': 400, 'x': 100, 'y': 200}
    returned_window_rect = self._driver.SetWindowRect(*target_window_rect)
    self.assertEqual(self._driver.GetWindowRect(), target_window_rect)
    self.assertEqual(returned_window_rect, target_window_rect_dict)

  def testWindowMaximize(self):
    old_rect_list = [640, 400, 100, 200]
    self._driver.SetWindowRect(*old_rect_list)
    new_rect = self._driver.MaximizeWindow()
    new_rect_list = [
        new_rect['width'],
        new_rect['height'],
        new_rect['x'],
        new_rect['y']
    ]
    self.assertNotEqual(old_rect_list, new_rect_list)

    self._driver.SetWindowRect(*old_rect_list)
    self.assertEqual(old_rect_list, self._driver.GetWindowRect())

  def testWindowMaximizeFromFrame(self):
    # This test is somewhat close to WindowTest.testCanMaximizeTheWindow of
    # Selenium in its attempt to reproduce https://crbug.com/chromedriver/2663
    self._http_server.SetDataForPath('/nested.html',
      bytes('<p>nested.html</p>', 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="/nested.html">', 'utf-8'))

    old_rect_list = [640, 400, 100, 200]
    self._driver.SetWindowRect(*old_rect_list)

    self._driver.Load(self.GetHttpUrlForFile('/main.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)

    new_rect = self._driver.MaximizeWindow()
    new_rect_list = [
        new_rect['width'],
        new_rect['height'],
        new_rect['x'],
        new_rect['y']
    ]
    self.assertNotEqual(old_rect_list, new_rect_list)

    self._driver.SetWindowRect(*old_rect_list)
    self.assertEqual(old_rect_list, self._driver.GetWindowRect())

  def testWindowMinimize(self):
    handle = self._driver.GetCurrentWindowHandle()
    self._driver.SetWindowRect(640, 400, 100, 200)
    rect = self._driver.MinimizeWindow()
    expected_rect = {'y': 200, 'width': 640, 'height': 400, 'x': 100}

    #check it returned the correct rect
    for key in expected_rect.keys():
      self.assertEqual(expected_rect[key], rect[key])

    # check its minimized
    res = self._driver.SendCommandAndGetResult('Browser.getWindowForTarget',
                                               {'targetId': handle})
    self.assertEqual('minimized', res['bounds']['windowState'])

  def testWindowFullScreen(self):
    old_rect_list = [640, 400, 100, 200]
    self._driver.SetWindowRect(*old_rect_list)
    self.assertEqual(self._driver.GetWindowRect(), old_rect_list)
    new_rect = self._driver.FullScreenWindow()
    new_rect_list = [
        new_rect['width'],
        new_rect['height'],
        new_rect['x'],
        new_rect['y']
    ]
    self.assertNotEqual(old_rect_list, new_rect_list)

    self._driver.SetWindowRect(*old_rect_list)
    for _ in range(10):
      if old_rect_list == self._driver.GetWindowRect():
        break
      time.sleep(0.1)
    self.assertEqual(old_rect_list, self._driver.GetWindowRect())

  def testWindowFullScreenHeadless(self):
    old_rect_list = self._driver.GetWindowRect()
    # Testing the resulting screensize doesn't work in headless, because there
    # is no screen to give a size.
    # We just want to ensure this command doesn't timeout or error.
    self._driver.FullScreenWindow()
    # Restore a known size so next tests won't fail
    self._driver.SetWindowRect(*old_rect_list)

  def testConsoleLogSources(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/console_log.html'))
    logs = self._driver.GetLog('browser')

    # The javascript and network logs can come in any order.
    if logs[0]['source'] == 'javascript':
        js_log = logs[0]
        network_log = logs[1]
    else:
        network_log = logs[0]
        js_log = logs[1]
    self.assertEqual('javascript', js_log['source'])
    self.assertTrue('TypeError' in js_log['message'])

    self.assertEqual('network', network_log['source'])
    self.assertTrue('nonexistent.png' in network_log['message'])
    self.assertTrue('404' in network_log['message'])

    # Sometimes, we also get an error for a missing favicon.
    if len(logs) > 2:
      self.assertEqual('network', logs[2]['source'])
      self.assertTrue('favicon.ico' in logs[2]['message'])
      self.assertTrue('404' in logs[2]['message'])
      self.assertEqual(3, len(logs))
    else:
      self.assertEqual(2, len(logs))

  def testPendingConsoleLog(self):
    new_logs = [""]
    def GetPendingLogs(driver):
      response = driver.GetLog('browser')
      new_logs[0] = [x for x in response if x['source'] == 'console-api']
      return new_logs[0]

    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/pending_console_log.html'))
    logs = self._driver.GetLog('browser')
    self.assertEqual('console-api', logs[0]['source'])
    self.assertTrue('"InitialError" 2018 "Third"' in logs[0]['message'])

    self.WaitForCondition(lambda: len(GetPendingLogs(self._driver)) > 0 , 6)
    self.assertEqual('console-api', new_logs[0][0]['source'])
    self.assertTrue('"RepeatedError" "Second" "Third"' in
                    new_logs[0][0]['message'])

  def testGetLogOnClosedWindow(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('css selector', '#link').Click()
    self.WaitForNewWindow(self._driver, old_handles)
    self._driver.CloseWindow()
    try:
      self._driver.GetLog('browser')
    except chromedriver.ChromeDriverException as e:
      self.fail('exception while calling GetLog on a closed tab: {}'.format(e))

  def testGetLogOnWindowWithAlert(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript('alert("alert!");')
    try:
      self._driver.GetLog('browser')
    except Exception as e:
      self.fail(str(e))

  def testDoesntHangOnDebugger(self):
    self._driver.Load('about:blank')
    self._driver.ExecuteScript('debugger;')

  def testChromeDriverSendLargeData(self):
    script = 'return "0".repeat(10e6);'
    lots_of_data = self._driver.ExecuteScript(script)
    self.assertEqual('0'.zfill(int(10e6)), lots_of_data)

  def testEmulateNetworkConditions(self):
    # Network conditions must be set before it can be retrieved.
    self.assertRaises(chromedriver.UnknownError,
                      self._driver.GetNetworkConditions)

    # DSL: 2Mbps throughput, 5ms RTT
    latency = 5
    throughput = 2048 * 1024
    self._driver.SetNetworkConditions(latency, throughput, throughput)

    network = self._driver.GetNetworkConditions()
    self.assertEqual(latency, network['latency'])
    self.assertEqual(throughput, network['download_throughput'])
    self.assertEqual(throughput, network['upload_throughput'])
    self.assertEqual(False, network['offline'])

    # Network Conditions again cannot be retrieved after they've been deleted.
    self._driver.DeleteNetworkConditions()
    self.assertRaises(chromedriver.UnknownError,
                      self._driver.GetNetworkConditions)

  def testEmulateNetworkConditionsName(self):
    # DSL: 2Mbps throughput, 5ms RTT
    # latency = 5
    # throughput = 2048 * 1024
    self._driver.SetNetworkConditionsName('DSL')

    network = self._driver.GetNetworkConditions()
    self.assertEqual(5, network['latency'])
    self.assertEqual(2048*1024, network['download_throughput'])
    self.assertEqual(2048*1024, network['upload_throughput'])
    self.assertEqual(False, network['offline'])

  def testEmulateNetworkConditionsSpeed(self):
    # Warm up the browser.
    self._http_server.SetDataForPath(
        '/', b"<html><body>blank</body></html>")
    self._driver.Load(self._http_server.GetUrl() + '/')

    # DSL: 2Mbps throughput, 5ms RTT
    latency = 5
    throughput_kbps = 2048
    throughput = throughput_kbps * 1024
    self._driver.SetNetworkConditions(latency, throughput, throughput)

    _32_bytes = " 0 1 2 3 4 5 6 7 8 9 A B C D E F"
    _1_megabyte = _32_bytes * 32768
    self._http_server.SetDataForPath(
        '/1MB',
        bytes("<html><body>%s</body></html>" % _1_megabyte, 'utf-8'))
    start = monotonic()
    self._driver.Load(self._http_server.GetUrl() + '/1MB')
    finish = monotonic()
    duration = finish - start
    actual_throughput_kbps = 1024 / duration
    self.assertLessEqual(actual_throughput_kbps, throughput_kbps * 1.5)
    self.assertGreaterEqual(actual_throughput_kbps, throughput_kbps / 1.5)

  def testEmulateNetworkConditionsNameSpeed(self):
    # Warm up the browser.
    self._http_server.SetDataForPath(
        '/', b"<html><body>blank</body></html>")
    self._driver.Load(self._http_server.GetUrl() + '/')

    # DSL: 2Mbps throughput, 5ms RTT
    throughput_kbps = 2048
    throughput = throughput_kbps * 1024
    self._driver.SetNetworkConditionsName('DSL')

    _32_bytes = " 0 1 2 3 4 5 6 7 8 9 A B C D E F"
    _1_megabyte = _32_bytes * 32768
    self._http_server.SetDataForPath(
        '/1MB',
        bytes("<html><body>%s</body></html>" % _1_megabyte, 'utf-8'))
    start = monotonic()
    self._driver.Load(self._http_server.GetUrl() + '/1MB')
    finish = monotonic()
    duration = finish - start
    actual_throughput_kbps = 1024 / duration
    self.assertLessEqual(actual_throughput_kbps, throughput_kbps * 1.5)
    self.assertGreaterEqual(actual_throughput_kbps, throughput_kbps / 1.5)

  def testEmulateNetworkConditionsOffline(self):
    # A workaround for crbug.com/177511; when setting offline, the throughputs
    # must be 0.
    self._driver.SetNetworkConditions(0, 0, 0, offline=True)
    self.assertRaises(chromedriver.ChromeDriverException,
                      self._driver.Load,
                      self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    # The "X is not available" title is set after the page load event fires, so
    # we have to explicitly wait for this to change. We can't rely on the
    # navigation tracker to block the call to Load() above.
    self.WaitForCondition(lambda: 'is not available' in self._driver.GetTitle())

  def testSendCommandAndGetResult(self):
    """Sends a custom command to the DevTools debugger and gets the result"""
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/page_test.html'))
    params = {}
    document = self._driver.SendCommandAndGetResult('DOM.getDocument', params)
    self.assertTrue('root' in document)

  def _FindElementInShadowDom(self, css_selectors):
    """Find an element inside shadow DOM using CSS selectors.
    The last item in css_selectors identify the element to find. All preceding
    selectors identify the hierarchy of shadow hosts to traverse in order to
    reach the target shadow DOM."""
    current = None
    for selector in css_selectors:
      if current is None:
        # First CSS selector, start from root DOM.
        current = self._driver
      else:
        # current is a shadow host selected previously.
        # Enter the corresponding shadow root.
        current = self._driver.ExecuteScript(
            'return arguments[0].shadowRoot', current)
      current = current.FindElement('css selector', selector)
    return current

  def testShadowDomFindElement(self):
    """Checks that chromedriver can find elements in a shadow DOM."""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    self.assertTrue(self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#textBox"]))

  def testShadowDomFindChildElement(self):
    """Checks that chromedriver can find child elements from a shadow DOM
    element."""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    elem = self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#childDiv"])
    self.assertTrue(elem.FindElement("css selector", "#textBox"))

  def testShadowDomFindElementFailsFromRoot(self):
    """Checks that chromedriver can't find elements in a shadow DOM from
    root."""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    # can't find element from the root without /deep/
    with self.assertRaises(chromedriver.NoSuchElement):
      self._driver.FindElement("css selector", "#textBox")

  def testShadowDomText(self):
    """Checks that chromedriver can find extract the text from a shadow DOM
    element."""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    elem = self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#heading"])
    self.assertEqual("Child", elem.GetText())

  def testShadowDomSendKeys(self):
    """Checks that chromedriver can call SendKeys on a shadow DOM element."""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    elem = self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#textBox"])
    elem.SendKeys("bar")
    self.assertEqual("foobar", self._driver.ExecuteScript(
        'return arguments[0].value;', elem))

  def testShadowDomClear(self):
    """Checks that chromedriver can call Clear on a shadow DOM element."""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    elem = self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#textBox"])
    elem.Clear()
    self.assertEqual("", self._driver.ExecuteScript(
        'return arguments[0].value;', elem))

  def testShadowDomClick(self):
    """Checks that chromedriver can call Click on an element in a shadow DOM."""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    # Wait for page to stabilize. See https://crbug.com/954553#c7
    time.sleep(1)
    elem = self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#button"])
    elem.Click()
    # the button's onClicked handler changes the text box's value
    self.assertEqual("Button Was Clicked", self._driver.ExecuteScript(
        'return arguments[0].value;',
        self._FindElementInShadowDom(["#innerDiv", "#parentDiv", "#textBox"])))

  def testShadowDomActionClick(self):
    '''Checks that ChromeDriver can use actions API to click on an element in a
    shadow DOM.'''
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    # Wait for page to stabilize. See https://crbug.com/954553#c7
    time.sleep(1)
    elem = self._FindElementInShadowDom(
        ['#innerDiv', '#parentDiv', '#button'])
    actions = ({'actions': [{
      'type': 'pointer',
      'actions': [{'type': 'pointerMove', 'x': 0, 'y': 0, 'origin': elem},
                  {'type': 'pointerDown', 'button': 0},
                  {'type': 'pointerUp', 'button': 0}],
      'id': 'pointer1'}]})
    self._driver.PerformActions(actions)
    # the button's onClicked handler changes the text box's value
    self.assertEqual('Button Was Clicked', self._driver.ExecuteScript(
        'return arguments[0].value;',
        self._FindElementInShadowDom(['#innerDiv', '#parentDiv', '#textBox'])))

  def testShadowDomStaleReference(self):
    """Checks that trying to manipulate shadow DOM elements that are detached
    from the document raises a StaleElementReference exception"""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    elem = self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#button"])
    self._driver.ExecuteScript(
        'document.querySelector("#outerDiv").innerHTML="<div/>";')
    with self.assertRaises(chromedriver.StaleElementReference):
      elem.Click()

  def testTouchDownMoveUpElement(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/touch_action_tests.html'))
    target = self._driver.FindElement('css selector', '#target')
    location = target.GetLocation()
    self._driver.TouchDown(location['x'], location['y'])
    events = self._driver.FindElement('css selector', '#events')
    self.assertEqual('events: touchstart', events.GetText())
    self._driver.TouchMove(location['x'] + 1, location['y'] + 1)
    self.assertEqual('events: touchstart touchmove', events.GetText())
    self._driver.TouchUp(location['x'] + 1, location['y'] + 1)
    self.assertEqual('events: touchstart touchmove touchend', events.GetText())

  def testGetElementRect(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/absolute_position_element.html'))
    target = self._driver.FindElement('css selector', '#target')
    rect = target.GetRect()
    self.assertEqual(18, rect['x'])
    self.assertEqual(10, rect['y'])
    self.assertEqual(200, rect['height'])
    self.assertEqual(210, rect['width'])

  def testTouchFlickElement(self):
    dx = 3
    dy = 4
    speed = 5
    flickTouchEventsPerSecond = 30
    moveEvents = int(
        math.sqrt(dx * dx + dy * dy) * flickTouchEventsPerSecond / speed)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("touchstart", function() {'
        '  div.innerHTML = "preMove0";'
        '});'
        'div.addEventListener("touchmove", function() {'
        '  res = div.innerHTML.match(/preMove(\d+)/);'
        '  if (res != null) {'
        '    div.innerHTML = "preMove" + (parseInt(res[1], 10) + 1);'
        '  }'
        '});'
        'div.addEventListener("touchend", function() {'
        '  if (div.innerHTML == "preMove' + str(moveEvents) + '") {'
        '    div.innerHTML = "new<br>";'
        '  }'
        '});'
        'return div;')
    self._driver.TouchFlick(div, dx, dy, speed)
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testSwitchesToTopFrameAfterNavigation(self):
    self._driver.Load('about:blank')
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/outer.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/outer.html'))
    p = self._driver.FindElement('tag name', 'p')
    self.assertEqual('Two', p.GetText())

  def testSwitchesToTopFrameAfterRefresh(self):
    self._driver.Load('about:blank')
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/outer.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    self._driver.Refresh()
    p = self._driver.FindElement('tag name', 'p')
    self.assertEqual('Two', p.GetText())

  def testSwitchesToTopFrameAfterGoingBack(self):
    self._driver.Load('about:blank')
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/outer.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/inner.html'))
    self._driver.GoBack()
    p = self._driver.FindElement('tag name', 'p')
    self.assertEqual('Two', p.GetText())

  def testCanSwitchToPrintPreviewDialog(self):
    old_handles = self._driver.GetWindowHandles()
    print("Test debug: actual len of old_handles: " + str(len(old_handles)),
            file = sys.stdout)
    self.assertEqual(1, len(old_handles))
    self._driver.ExecuteScript('setTimeout(function(){window.print();}, 0);')
    new_window_handle = self.WaitForNewWindow(self._driver, old_handles)
    if new_window_handle is None:
      print("Test debug: new_window_handle is None", file = sys.stdout)
    else:
      print("Test debug: new_window_handle is not None", file = sys.stdout)

    self.assertNotEqual(None, new_window_handle)
    self._driver.SwitchToWindow(new_window_handle)
    print("Test debug: actual GetCurrentUrl: " + self._driver.GetCurrentUrl(),
            file = sys.stdout)

    self.assertEqual('chrome://print/', self._driver.GetCurrentUrl())

  def testCanClickInIframes(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/nested.html'))
    a = self._driver.FindElement('tag name', 'a')
    a.Click()
    frame_url = self._driver.ExecuteScript('return window.location.href')
    self.assertTrue(frame_url.endswith('#one'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    a = self._driver.FindElement('tag name', 'a')
    a.Click()
    frame_url = self._driver.ExecuteScript('return window.location.href')
    self.assertTrue(frame_url.endswith('#two'))

  def testDoesntHangOnFragmentNavigation(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html#x'))

  def SetCookie(self, request):
    return {'Set-Cookie': 'x=y; HttpOnly'}, b"<!DOCTYPE html><html></html>"

  def testGetHttpOnlyCookie(self):
    self._http_server.SetCallbackForPath('/setCookie', self.SetCookie)
    self._driver.Load(self.GetHttpUrlForFile('/setCookie'))
    self._driver.AddCookie({'name': 'a', 'value': 'b'})
    cookies = self._driver.GetCookies()
    self.assertEqual(2, len(cookies))
    for cookie in cookies:
      self.assertIn('name', cookie)
      if cookie['name'] == 'a':
        self.assertFalse(cookie['httpOnly'])
      elif cookie['name'] == 'x':
        self.assertTrue(cookie['httpOnly'])
      else:
        self.fail('unexpected cookie: %s' % json.dumps(cookie))

  def testCookiePath(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/long_url/empty.html'))
    self._driver.AddCookie({'name': 'a', 'value': 'b'})
    self._driver.AddCookie({
        'name': 'x', 'value': 'y', 'path': '/chromedriver/long_url'})
    cookies = self._driver.GetCookies()
    self.assertEqual(2, len(cookies))
    for cookie in cookies:
      self.assertIn('path', cookie)
      if cookie['name'] == 'a':
        self.assertEqual('/' , cookie['path'])
      if cookie['name'] == 'x':
        self.assertEqual('/chromedriver/long_url' , cookie['path'])

  def testGetNamedCookie(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/empty.html'))
    self._driver.AddCookie({'name': 'a', 'value': 'b'})
    named_cookie = self._driver.GetNamedCookie('a')
    self.assertEqual('a' , named_cookie['name'])
    self.assertEqual('b' , named_cookie['value'])
    self.assertRaisesRegex(
        chromedriver.NoSuchCookie, "no such cookie",
        self._driver.GetNamedCookie, 'foo')

  def testDeleteCookie(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/empty.html'))
    self._driver.AddCookie({'name': 'a', 'value': 'b'})
    self._driver.AddCookie({'name': 'x', 'value': 'y'})
    self._driver.AddCookie({'name': 'p', 'value': 'q'})
    cookies = self._driver.GetCookies()
    self.assertEqual(3, len(cookies))
    self._driver.DeleteCookie('a')
    self.assertEqual(2, len(self._driver.GetCookies()))
    self._driver.DeleteAllCookies()
    self.assertEqual(0, len(self._driver.GetCookies()))

  def testCookieForFrame(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/cross_domain_iframe.html'))
    self._driver.AddCookie({'name': 'outer', 'value': 'main context'})

    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    self.assertTrue(self.WaitForCondition(
        lambda: 'outer.html' in
                self._driver.ExecuteScript('return window.location.href')))
    self._driver.AddCookie({'name': 'inner', 'value': 'frame context'})
    cookies = self._driver.GetCookies()
    self.assertEqual(1, len(cookies))
    self.assertEqual('inner', cookies[0]['name'])

    self._driver.SwitchToMainFrame()
    cookies = self._driver.GetCookies()
    self.assertEqual(1, len(cookies))
    self.assertEqual('outer', cookies[0]['name'])

  def testCanClickAlertInIframes(self):
    # This test requires that the page be loaded from a file:// URI, rather than
    # the test HTTP server.
    path = os.path.join(chrome_paths.GetTestData(), 'chromedriver',
      'page_with_frame.html')
    url = 'file://' + urllib.request.pathname2url(path)
    self._driver.Load(url)
    frame = self._driver.FindElement('css selector', '#frm')
    self._driver.SwitchToFrame(frame)
    a = self._driver.FindElement('css selector', '#btn')
    a.Click()
    self.WaitForCondition(lambda: self._driver.IsAlertOpen())
    self._driver.HandleAlert(True)

  def testThrowErrorWithExecuteScript(self):
    self.assertRaisesRegex(
        chromedriver.JavaScriptError, "some error",
        self._driver.ExecuteScript, 'throw new Error("some error")')

  def testDoesntCrashWhenScriptLogsUndefinedValue(self):
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1547
    self._driver.ExecuteScript('var b; console.log(b);')

  def testDoesntThrowWhenPageLogsUndefinedValue(self):
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1547
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/log_undefined_value.html'))

  def testCanSetCheckboxWithSpaceKey(self):
     self._driver.Load('about:blank')
     self._driver.ExecuteScript(
         "document.body.innerHTML = '<input type=\"checkbox\">';")
     checkbox = self._driver.FindElement('tag name', 'input')
     self.assertFalse(
         self._driver.ExecuteScript('return arguments[0].checked', checkbox))
     checkbox.SendKeys(' ')
     self.assertTrue(
         self._driver.ExecuteScript('return arguments[0].checked', checkbox))

  def testElementReference(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/element_ref.html'))
    element = self._driver.FindElement('css selector', '#link')
    self.assertRegex(element._id, _ELEMENT_REF_REGEX,
                     msg='Element id format is incorrect')

  def testElementReferenceViaScript(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/element_ref.html'))
    element = self._driver.ExecuteScript(
        'return document.getElementById("link")')
    self.assertRegex(element._id, _ELEMENT_REF_REGEX,
                     msg='Element id format is incorrect')

  def testElementReferenceNoNavigation(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    self.assertRegex(div._id, _ELEMENT_REF_REGEX,
                     msg='Element id format is incorrect')

  def testElementReferenceInNewWindow(self):
    # We need to run this test in a new tab so that it is isolated from previous
    # test runs.
    old_windows = self._driver.GetWindowHandles()
    self._driver.ExecuteScript('window.open("about:blank")')
    new_window = self.WaitForNewWindow(self._driver, old_windows)
    self._driver.SwitchToWindow(new_window)
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    self.assertRegex(div._id, _ELEMENT_REF_REGEX,
                     msg='Element id format is incorrect')

  def testFindElementWhenElementIsOverridden(self):
    self._driver.Load('about:blank')
    self._driver.ExecuteScript(
        'document.body.appendChild(document.createElement("a"));')
    self._driver.ExecuteScript('window.Element = {}')
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'a')))

  def testExecuteScriptWhenObjectPrototypeIsModified(self):
    # Some JavaScript libraries (e.g. MooTools) do things like this. For context
    # see https://bugs.chromium.org/p/chromedriver/issues/detail?id=1521
    self._driver.Load('about:blank')
    self._driver.ExecuteScript('Object.prototype.$family = undefined;')
    self.assertEqual(1, self._driver.ExecuteScript('return 1;'))

  def testWebWorkerFrames(self):
    """Verify web worker frames are handled correctly.

    Regression test for bug
    https://bugs.chromium.org/p/chromedriver/issues/detail?id=2340.
    The bug was triggered by opening a page with web worker, and then opening a
    page on a different site. We simulate a different site by using 'localhost'
    as the host name (default is '127.0.0.1').
    """
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/web_worker.html'))
    self._driver.Load(self._http_server.GetUrl('localhost')
                      + '/chromedriver/empty.html')

  def testWaitForCurrentFrameToLoad(self):
    """Verify ChromeDriver waits for loading events of current frame
    Regression test for bug
    https://bugs.chromium.org/p/chromedriver/issues/detail?id=3164
    Clicking element in frame triggers reload of that frame, click should not
    return until loading is complete.
    """
    def waitAndRespond():
      # test may not detect regression without small sleep.
      # locally, .2 didn't fail before code change, .3 did
      time.sleep(.5)
      self._sync_server.RespondWithContent(
          b"""
          <html>
            <body>
              <p id='valueToRead'>11</p>
            </body>
          </html>
          """)

    self._http_server.SetDataForPath('/page10.html',
      bytes("""
      <html>
        <head>
          <title>
            Frame
          </title>
          <script>
            function reloadWith(i) {
              window.location.assign('%s');
            }
          </script>
        </head>
        <body>
          <button id='prev' onclick="reloadWith(9)">-1</button>
          <button id='next' onclick="reloadWith(11)">+1</button>
          <p id='valueToRead'>10</p>
        </body>
      </html>
       """ % self._sync_server.GetUrl(), 'utf-8'))
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/page_for_next_iframe.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    thread = threading.Thread(target=waitAndRespond)
    thread.start()
    self._driver.FindElement('css selector', '#next').Click()
    value_display = self._driver.FindElement('css selector', '#valueToRead')
    self.assertEqual('11', value_display.GetText())

  def testSlowIFrame(self):
    """Verify ChromeDriver does not wait for slow frames to load.
    Regression test for bugs
    https://bugs.chromium.org/p/chromedriver/issues/detail?id=2198 and
    https://bugs.chromium.org/p/chromedriver/issues/detail?id=2350.
    """
    def waitAndRespond():
      # Send iframe contents slowly
      time.sleep(3)
      self._sync_server.RespondWithContent(
        b'<html><div id=iframediv>IFrame contents</div></html>')

    self._http_server.SetDataForPath('/top.html',
        bytes("""
        <html><body>
        <div id='top'>
          <input id='button' type="button" onclick="run()" value='Click'>
        </div>
        <script>
        function run() {
          var iframe = document.createElement('iframe');
          iframe.id = 'iframe';
          iframe.setAttribute('src', '%s');
          document.body.appendChild(iframe);
        }
        </script>
        </body></html>""" % self._sync_server.GetUrl(), 'utf-8'))
    self._driver.Load(self._http_server.GetUrl() + '/top.html')
    thread = threading.Thread(target=waitAndRespond)
    thread.start()
    start = monotonic()
    # Click should not wait for frame to load, so elapsed time from this
    # command should be < 2 seconds.
    self._driver.FindElement('css selector', '#button').Click()
    self.assertLess(monotonic() - start, 2.0)
    frame = self._driver.FindElement('css selector', '#iframe')
    # WaitForPendingNavigations examines the load state of the current frame
    # so ChromeDriver will wait for frame to load after SwitchToFrame
    # start is reused because that began the pause for the frame load
    self._driver.SwitchToFrame(frame)
    self.assertGreaterEqual(monotonic() - start, 2.0)
    self._driver.FindElement('css selector', '#iframediv')
    thread.join()

  @staticmethod
  def MakeRedImageTestScript(png_data_in_base64):
    """Used by the takeElementScreenshot* tests to load the PNG image via a data
    URI, analyze it, and PASS/FAIL depending on whether all the pixels are all
    rgb(255,0,0)."""
    return (
        """
        const resolve = arguments[arguments.length - 1];
        const image = new Image();
        image.onload = () => {
          var canvas = document.createElement('canvas');
          canvas.width = image.width;
          canvas.height = image.height;
          var context = canvas.getContext('2d');
          context.drawImage(image, 0, 0);
          const pixels =
              context.getImageData(0, 0, image.width, image.height).data;
          for (let i = 0; i < pixels.length; i += 4) {
            if (pixels[i + 0] != 255 ||  // Red
                pixels[i + 1] != 0 ||    // Green
                pixels[i + 2] != 0) {    // Blue
              const message = (
                  'FAIL: Bad pixel rgb(' + pixels.slice(i, i + 3).join(',') +
                  ') at offset ' + i + ' from ' + image.src);
              // "Disabled" on Mac 10.10: 1/15 test runs produces an incorrect
              // pixel. Since no later Mac version, nor any other platform,
              // exhibits this problem, we assume this is due to a bug in this
              // specific version of Mac OS. So, just log the error and pass
              // the test. http://crbug.com/913603
              if (navigator.userAgent.indexOf('Mac OS X 10_10') != -1) {
                console.error(message);
                console.error('Passing test due to Mac 10.10-specific bug.');
                resolve('PASS');
              } else {
                resolve(message);
              }
              return;
            }
          }
          resolve('PASS');
        };
        image.src = 'data:image/png;base64,%s';
        """ % png_data_in_base64.replace("'", "\\'"))

  def takeScreenshotAndVerifyCorrect(self, element):
      """ Takes screenshot of given element and returns
      'PASS' if all pixels in screenshot are rgb(255, 0, 0)
      and 'FAIL' otherwise
      """
      elementScreenshotPNGBase64 = element.TakeElementScreenshot()
      self.assertIsNotNone(elementScreenshotPNGBase64)
      return self._driver.ExecuteAsyncScript(
          ChromeDriverTest.MakeRedImageTestScript(elementScreenshotPNGBase64))

  def testTakeElementScreenshot(self):
    self._driver.Load(self.GetHttpUrlForFile(
                      '/chromedriver/page_with_redbox.html'))
    # Wait for page to stabilize in case of Chrome showing top bars.
    # See https://crbug.com/chromedriver/2986
    time.sleep(1)
    redElement = self._driver.FindElement('css selector', '#box')
    analysisResult = self.takeScreenshotAndVerifyCorrect(redElement)
    self.assertEqual('PASS', analysisResult)

  def testTakeElementScreenshotPartlyVisible(self):
    self._driver.Load(self.GetHttpUrlForFile(
                      '/chromedriver/page_with_redbox_partly_visible.html'))
    self._driver.SetWindowRect(500, 500, 0, 0)
    # Wait for page to stabilize. See https://crbug.com/chromedriver/2986
    time.sleep(1)
    redElement = self._driver.FindElement('css selector', '#box')
    analysisResult = self.takeScreenshotAndVerifyCorrect(redElement)
    self.assertEqual('PASS', analysisResult)

  def testTakeElementScreenshotInIframe(self):
    self._driver.Load(self.GetHttpUrlForFile(
                      '/chromedriver/page_with_iframe_redbox.html'))
    frame = self._driver.FindElement('css selector', '#frm')
    self._driver.SwitchToFrame(frame)
    # Wait for page to stabilize in case of Chrome showing top bars.
    # See https://crbug.com/chromedriver/2986
    time.sleep(1)
    redElement = self._driver.FindElement('css selector', '#box')
    analysisResult = self.takeScreenshotAndVerifyCorrect(redElement)
    self.assertEqual('PASS', analysisResult)

  def testTakeLargeElementScreenshot(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/large_element.html'))
    self._driver.SetWindowRect(500, 500, 0, 0)
    # Wait for page to stabilize. See https://crbug.com/chromedriver/2986
    time.sleep(1)
    redElement = self._driver.FindElement('css selector', '#A')
    analysisResult = self.takeScreenshotAndVerifyCorrect(redElement)
    self.assertEqual('PASS', analysisResult)

  @staticmethod
  def png_dimensions(png_data_in_base64):
    image = base64.b64decode(png_data_in_base64)
    width, height = struct.unpack('>LL', image[16:24])
    return int(width), int(height)


  def testTakeLargeElementViewportScreenshot(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/large_element.html'))
    self._driver.SetWindowRect(640, 400, 0, 0)
    # Wait for page to stabilize. See https://crbug.com/chromedriver/2986
    time.sleep(1)
    viewportScreenshotPNGBase64  = self._driver.TakeScreenshot()
    self.assertIsNotNone(viewportScreenshotPNGBase64)
    mime_type = imghdr.what('', base64.b64decode(viewportScreenshotPNGBase64))
    self.assertEqual('png', mime_type)
    image_width, image_height = self.png_dimensions(viewportScreenshotPNGBase64)
    viewport_width, viewport_height = self._driver.ExecuteScript(
        '''
        const {devicePixelRatio, innerHeight, innerWidth} = window;

        return [
          Math.floor(innerWidth * devicePixelRatio),
          Math.floor(innerHeight * devicePixelRatio)
        ];
        ''')
    self.assertEqual(image_width, viewport_width)
    self.assertEqual(image_height, viewport_height)

  def testTakeLargeElementFullPageScreenshot(self):
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/large_element.html'))
    width = 640
    height = 400
    self._driver.SetWindowRect(width, height, 0, 0)
    # Wait for page to stabilize. See https://crbug.com/chromedriver/2986
    time.sleep(1)
    fullpageScreenshotPNGBase64  = self._driver.TakeFullPageScreenshot()
    self.assertIsNotNone(fullpageScreenshotPNGBase64)
    mime_type = imghdr.what('', base64.b64decode(fullpageScreenshotPNGBase64))
    self.assertEqual('png', mime_type)
    image_width, image_height = self.png_dimensions(fullpageScreenshotPNGBase64)
    # According to https://javascript.info/size-and-scroll-window,
    # width/height of the whole document, with the scrolled out part
    page_width, page_height = self._driver.ExecuteScript(
        '''
        const body = document.body;
        const doc = document.documentElement;
        const width = Math.max(body.scrollWidth, body.offsetWidth,\
                               body.clientWidth, doc.scrollWidth,\
                               doc.offsetWidth, doc.clientWidth);
        const height = Math.max(body.scrollHeight, body.offsetHeight,\
                                body.clientHeight, doc.scrollHeight,\
                                doc.offsetHeight, doc.clientHeight);

        return [
          width,
          height
        ];
        ''')
    self.assertEqual(image_width, page_width)
    self.assertEqual(image_height, page_height)

    # Assert Window Rect size stay the same after taking fullpage screenshot
    size = self._driver.GetWindowRect()
    self.assertEqual(size[0], width)
    self.assertEqual(size[1], height)

    # Verify scroll bars presence after test
    horizontal_scroll_bar, vertical_scroll_bar = self._driver.ExecuteScript(
        '''
        const doc = document.documentElement;

        return [
          doc.scrollWidth > doc.clientWidth,
          doc.scrollHeight > doc.clientHeight
        ];
        ''')
    self.assertEqual(horizontal_scroll_bar, True)
    self.assertEqual(vertical_scroll_bar, True)

  def testPrint(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    pdf = self._driver.PrintPDF({
                                  'orientation': 'landscape',
                                  'scale': 1.1,
                                  'margin': {
                                    'top': 1.1,
                                    'bottom': 2.2,
                                    'left': 3.3,
                                    'right': 4.4
                                  },
                                  'background': True,
                                  'shrinkToFit': False,
                                  'pageRanges': [1],
                                  'page': {
                                    'width': 15.6,
                                    'height': 20.6
                                  }
                                })
    decoded_pdf = base64.b64decode(pdf)
    self.assertTrue(decoded_pdf.startswith(b'%PDF'))
    self.assertTrue(decoded_pdf.endswith(b'%%EOF\n'))

  def testPrintInvalidArgument(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self.assertRaises(chromedriver.InvalidArgument,
                      self._driver.PrintPDF, {'pageRanges': ['x-y']})

  def testGenerateTestReport(self):
    self._driver.Load(self.GetHttpUrlForFile(
                      '/chromedriver/reporting_observer.html'))
    self._driver.GenerateTestReport('test report message')
    report = self._driver.ExecuteScript('return window.result;')

    self.assertEqual('test', report['type'])
    self.assertEqual('test report message', report['body']['message'])

  def testSetTimeZone(self):
    defaultTimeZoneScript = '''
       return (new Intl.DateTimeFormat()).resolvedOptions().timeZone
       '''
    localHourScript = '''
       return (new Date("2020-10-10T00:00:00Z")).getHours()
       '''

    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))

    # Test to switch to Taipei
    self._driver.SetTimeZone('Asia/Taipei')
    timeZone = self._driver.ExecuteScript(defaultTimeZoneScript)
    self.assertEqual('Asia/Taipei', timeZone)
    localHour = self._driver.ExecuteScript(localHourScript)
    # Taipei time is GMT+8. Not observes DST.
    self.assertEqual(8, localHour)

    # Test to switch to Tokyo
    self._driver.SetTimeZone('Asia/Tokyo')
    timeZone = self._driver.ExecuteScript(defaultTimeZoneScript)
    self.assertEqual('Asia/Tokyo', timeZone)
    localHour = self._driver.ExecuteScript(localHourScript)
    # Tokyo time is GMT+9. Not observes DST.
    self.assertEqual(9, localHour)

  def GetPermissionWithQuery(self, query):
    script = """
        let query = arguments[0];
        let done = arguments[1];
        console.log(done);
        navigator.permissions.query(query)
          .then(function(value) {
              done({ status: 'success', value: value && value.state });
            }, function(error) {
              done({ status: 'error', value: error && error.message });
            });
    """
    return self._driver.ExecuteAsyncScript(script, query)

  def GetPermission(self, name):
    return self.GetPermissionWithQuery({ 'name': name })

  def CheckPermission(self, response, expected_state):
    self.assertEqual(response['status'], 'success')
    self.assertEqual(response['value'], expected_state)

  def testPermissionsOpaqueOriginsThrowError(self):
    """ Confirms that opaque origins cannot have overrides. """
    self._driver.Load("about:blank")
    self.assertRaises(chromedriver.InvalidArgument,
      self._driver.SetPermission, {'descriptor': { 'name': 'geolocation' },
        'state': 'denied'})

  def testPermissionStates(self):
    """ Confirms that denied, granted, and prompt can be set. """
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.SetPermission({
      'descriptor': { 'name': 'geolocation' },
      'state': 'denied'
    })
    self.CheckPermission(self.GetPermission('geolocation'), 'denied')
    self._driver.SetPermission({
      'descriptor': { 'name': 'geolocation' },
      'state': 'granted'
    })
    self.CheckPermission(self.GetPermission('geolocation'), 'granted')
    self._driver.SetPermission({
      'descriptor': { 'name': 'geolocation' },
      'state': 'prompt'
    })
    self.CheckPermission(self.GetPermission('geolocation'), 'prompt')

  def testSettingPermissionDoesNotAffectOthers(self):
    """ Confirm permissions do not affect unset permissions. """
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    response = self.GetPermission('geolocation')
    self.assertEqual(response['status'], 'success')
    status = response['value']
    self._driver.SetPermission({
      'descriptor': { 'name': 'background-sync' },
      'state': 'denied'
    })
    self.CheckPermission(self.GetPermission('background-sync'), 'denied')
    self.CheckPermission(self.GetPermission('geolocation'), status)

  def testMultiplePermissions(self):
    """ Confirms multiple custom permissions can be set simultaneously. """
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.SetPermission({
      'descriptor': { 'name': 'geolocation' },
      'state': 'denied'
    })
    self._driver.SetPermission({
      'descriptor': { 'name': 'background-fetch' },
      'state': 'prompt'
    })
    self._driver.SetPermission({
      'descriptor': { 'name': 'background-sync' },
      'state': 'granted'
    })
    self.CheckPermission(self.GetPermission('geolocation'), 'denied')
    self.CheckPermission(self.GetPermission('background-fetch'), 'prompt')
    self.CheckPermission(self.GetPermission('background-sync'), 'granted')

  def testSensorPermissions(self):
    """ Tests sensor permissions.

    Currently, Chrome controls all sensor permissions (accelerometer,
    magnetometer, gyroscope, ambient-light-sensor) with the 'sensors'
    permission. This test demonstrates this internal implementation detail so
    developers are aware of this behavior.
    """
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    parameters = {
      'descriptor': { 'name': 'magnetometer' },
      'state': 'granted'
    }
    self._driver.SetPermission(parameters)
    # Light sensor is not enabled by default, so it cannot be queried or set.
    #self.CheckPermission(self.GetPermission('ambient-light-sensor'), 'granted')
    self.CheckPermission(self.GetPermission('magnetometer'), 'granted')
    self.CheckPermission(self.GetPermission('accelerometer'), 'granted')
    self.CheckPermission(self.GetPermission('gyroscope'), 'granted')
    parameters = {
      'descriptor': { 'name': 'gyroscope' },
      'state': 'denied'
    }
    self._driver.SetPermission(parameters)
    #self.CheckPermission(self.GetPermission('ambient-light-sensor'), 'denied')
    self.CheckPermission(self.GetPermission('magnetometer'), 'denied')
    self.CheckPermission(self.GetPermission('accelerometer'), 'denied')
    self.CheckPermission(self.GetPermission('gyroscope'), 'denied')

  def testMidiPermissions(self):
    """ Tests midi permission requirements.

    MIDI, sysex: true, when granted, should automatically grant regular MIDI
    permissions.
    When regular MIDI is denied, this should also imply MIDI with sysex is
    denied.
    """
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    parameters = {
      'descriptor': { 'name': 'midi', 'sysex': True },
      'state': 'granted'
    }
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermissionWithQuery(parameters['descriptor']),
                         'granted')
    parameters['descriptor']['sysex'] = False
    self.CheckPermission(self.GetPermissionWithQuery(parameters['descriptor']),
                         'granted')

    parameters = {
      'descriptor': { 'name': 'midi', 'sysex': False },
      'state': 'denied'
    }
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermissionWithQuery(parameters['descriptor']),
                         'denied')
    # While this should be denied, Chrome does not do this.
    # parameters['descriptor']['sysex'] = True should be denied.

  def testClipboardPermissions(self):
    """ Tests clipboard permission requirements.

    clipboard-read with allowWithoutSanitization: true or false, and
    clipboard-write with allowWithoutSanitization: true are bundled together
    into one CLIPBOARD_READ_WRITE permission.

    clipboard write with allowWithoutSanitization: false is an auto-granted
    permission.
    """
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    parameters = {
      'descriptor': {
        'name': 'clipboard-read' ,
        'allowWithoutSanitization': False
      },
      'state': 'granted'
    }
    raw_write_parameters = {
      'descriptor': {
        'name': 'clipboard-write',
        'allowWithoutSanitization': True
      }
    }

    self.CheckPermission(self.GetPermissionWithQuery(parameters['descriptor']),
                        'prompt')
    self.CheckPermission(self.GetPermissionWithQuery(
                          raw_write_parameters['descriptor']), 'prompt')

    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermissionWithQuery(parameters['descriptor']),
                        'granted')
    parameters['descriptor']['allowWithoutSanitization'] = True
    self.CheckPermission(self.GetPermissionWithQuery(parameters['descriptor']),
                        'granted')
    parameters['descriptor']['name'] = 'clipboard-write'
    self.CheckPermission(self.GetPermissionWithQuery(parameters['descriptor']),
                        'granted')

    parameters = {
      'descriptor': { 'name': 'clipboard-write' },
      'state': 'prompt'
    }
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermission('clipboard-read'), 'granted')
    self.CheckPermission(self.GetPermission('clipboard-write'), 'prompt')

  def testPersistentStoragePermissions(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    parameters = {
      'descriptor': { 'name': 'persistent-storage' },
      'state': 'granted'
    }
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermission('persistent-storage'), 'granted')
    parameters['state'] = 'denied'
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermission('persistent-storage'), 'denied')

  def testPushAndNotificationsPermissions(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    parameters = {
      'descriptor': { 'name': 'notifications' },
      'state': 'granted'
    }
    push_descriptor = {
      'name': 'push',
      'userVisibleOnly': True
    }
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermission('notifications'), 'granted')
    self.CheckPermission(self.GetPermissionWithQuery(push_descriptor),
                         'granted')
    parameters['state'] = 'denied'
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermission('notifications'), 'denied')
    self.CheckPermission(self.GetPermissionWithQuery(push_descriptor), 'denied')
    push_descriptor['userVisibleOnly'] = False
    parameters = {
      'descriptor': push_descriptor,
      'state': 'prompt'
    }
    self.assertRaises(chromedriver.InvalidArgument,
                      self._driver.SetPermission, parameters)

  def testPermissionsSameOrigin(self):
    """ Assures permissions are shared between same-domain windows. """
    window_handle = self._driver.NewWindow()['handle']
    self._driver.SwitchToWindow(window_handle)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/link_nav.html'))
    another_window_handle = self._driver.NewWindow()['handle']
    self._driver.SwitchToWindow(another_window_handle)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))

    # Set permission.
    parameters = { 'descriptor': { 'name': 'geolocation' }, 'state': 'granted' }

    # Test that they are present across the same domain.
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermission('geolocation'), 'granted')
    self._driver.SwitchToWindow(window_handle)
    self.CheckPermission(self.GetPermission('geolocation'), 'granted')

  def testNewWindowSameDomainHasSamePermissions(self):
    """ Assures permissions are shared between same-domain windows, even when
    window is created after permissions are set. """
    window_handle = self._driver.NewWindow()['handle']
    self._driver.SwitchToWindow(window_handle)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.SetPermission({ 'descriptor': { 'name': 'geolocation' },
                                  'state': 'denied' })
    self.CheckPermission(self.GetPermission('geolocation'), 'denied')
    same_domain = self._driver.NewWindow()['handle']
    self._driver.SwitchToWindow(same_domain)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/link_nav.html'))
    self.CheckPermission(self.GetPermission('geolocation'), 'denied')


  def testPermissionsSameOriginDoesNotAffectOthers(self):
    """ Tests whether permissions set between two domains affect others. """
    window_handle = self._driver.NewWindow()['handle']
    self._driver.SwitchToWindow(window_handle)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/link_nav.html'))
    another_window_handle = self._driver.NewWindow()['handle']
    self._driver.SwitchToWindow(another_window_handle)
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    different_domain = self._driver.NewWindow()['handle']
    self._driver.SwitchToWindow(different_domain)
    self._driver.Load('https://google.com')
    self._driver.SetPermission({ 'descriptor': {'name': 'geolocation'},
                                  'state': 'denied' })

    # Switch for permissions.
    self._driver.SwitchToWindow(another_window_handle)

    # Set permission.
    parameters = { 'descriptor': { 'name': 'geolocation' }, 'state': 'prompt' }

    # Test that they are present across the same domain.
    self._driver.SetPermission(parameters)
    self.CheckPermission(self.GetPermission('geolocation'), 'prompt')

    self._driver.SwitchToWindow(window_handle)
    self.CheckPermission(self.GetPermission('geolocation'), 'prompt')

    # Assert different domain is not the same.
    self._driver.SwitchToWindow(different_domain)
    self.CheckPermission(self.GetPermission('geolocation'), 'denied')

  # Tests that the webauthn capabilities are true on desktop and false on
  # android.
  def testWebauthnVirtualAuthenticatorsCapability(self):
    is_desktop = _ANDROID_PACKAGE_KEY is None
    self.assertEqual(
        is_desktop,
        self._driver.capabilities['webauthn:virtualAuthenticators'])
    for extension in ['largeBlob', 'minPinLength', 'credBlob', 'prf']:
      self.assertEqual(
          is_desktop,
          self._driver.capabilities['webauthn:extension:' + extension])

  # Tests that the fedcm capability is true.
  def testFedCmCapability(self):
    self.assertEqual(
        True,
        self._driver.capabilities['fedcm:accounts'])

  def testCanClickInIframesInShadow(self):
    """Test that you can interact with a iframe within a shadow element.
       See https://bugs.chromium.org/p/chromedriver/issues/detail?id=3445
    """
    self._driver.SetTimeouts({'implicit': 2000})
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_iframe.html'))
    frame = self._driver.ExecuteScript(
      '''return document.querySelector("#shadow")
          .shadowRoot.querySelector("iframe")''')
    self._driver.SwitchToFrame(frame)
    message = self._driver.FindElement('css selector', '#message')
    self.assertTrue('clicked' not in message.GetText())
    button = self._driver.FindElement('tag name', 'button')
    button.Click()
    message = self._driver.FindElement('css selector', '#message.result')
    self.assertTrue('clicked' in message.GetText())

  def testCanClickInIframesInShadowScrolled(self):
    """Test that you can interact with a scrolled iframe
       within a scrolled shadow element.
       See https://bugs.chromium.org/p/chromedriver/issues/detail?id=3445
    """
    self._driver.SetTimeouts({'implicit': 2000})
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_iframe.html'))
    frame = self._driver.ExecuteScript(
      '''return document.querySelector("#shadow_scroll")
          .shadowRoot.querySelector("iframe")''')
    self._driver.SwitchToFrame(frame)
    message = self._driver.FindElement('css selector', '#message')
    self.assertTrue('clicked' not in message.GetText())
    button = self._driver.FindElement('tag name', 'button')
    button.Click()
    message = self._driver.FindElement('css selector', '#message.result')
    self.assertTrue('clicked' in message.GetText())

  def testBrowserNameIsEchoed(self):
    """Verifies that the returned browserName capability is the same as the
    sent browserName capability. The feature is relied upon by
    test_merge_browserName Web Platform Test.
    """
    browser_name = self._driver.capabilities['browserName']
    self.assertEqual(browser_name, _BROWSER_NAME)
    # The 'browserName' capability must always present in accordance with the
    # standard: https://w3c.github.io/webdriver/#capabilities
    self.assertIsNotNone(browser_name)
    # As product name is communicated differently under WebSockets and pipes
    # there is need to test both of these communication modes.
    # Test the detected browserName with remote-debugging-pipe.
    driver = self.CreateDriver(browser_name=browser_name,
                               chrome_switches=['--remote-debugging-pipe'])
    self.assertEqual(browser_name, driver.capabilities['browserName'])
    driver.Quit()
    # Test the detected browserName with remote-debugging-port.
    driver = self.CreateDriver(browser_name=browser_name,
                               chrome_switches=['--remote-debugging-port=0'])
    self.assertEqual(_BROWSER_NAME, driver.capabilities['browserName'])
    driver.Quit()

  def testBrowserNameHeadlessMode(self):
    """Verifies that the returned browserName capability for headless Chrome
    (not chrome-headless-shell) is 'chrome'.
    """
    # As product name is communicated differently under WebSockets and pipes
    # there is need to test both of these communication modes.
    # Test the detected browserName with remote-debugging-pipe.
    driver = self.CreateDriver(browser_name=browser_name,
                               chrome_switches=['--headless=new',
                                                '--remote-debugging-pipe'])
    self.assertEqual(_BROWSER_NAME, driver.capabilities['browserName'])
    driver.Quit()
    # Test the detected browserName with remote-debugging-port.
    driver = self.CreateDriver(browser_name=browser_name,
                               chrome_switches=['--headless=new',
                                                '--remote-debugging-port=0'])
    self.assertEqual(_BROWSER_NAME, driver.capabilities['browserName'])
    driver.Quit()

  def testHeadlessWithUserDataDirStarts(self):
    """Tests that ChromeDriver can launch chrome-headless-shell
       with user-data-dir provided as a command line argument.
       See https://bugs.chromium.org/p/chromedriver/issues/detail?id=4357
    """
    temp_dir = self.CreateTempDir()
    driver = self.CreateDriver(chrome_switches=[
                                   '--user-data-dir=%s' % temp_dir,
                               ])
    self.assertEqual(driver.GetTitle(), '')

  def testHeadlessWithExistingUserDataDirStarts(self):
    """Tests that ChromeDriver can launch chrome-headless-shell
       with user-data-dir, that already contains user data,
       provided as a command line argument
       See https://bugs.chromium.org/p/chromedriver/issues/detail?id=4357
    """
    temp_dir = self.CreateTempDir()
    driver = self.CreateDriver(chrome_switches=[
                                   '--user-data-dir=%s' % temp_dir,
                               ])
    self.assertEqual(driver.GetTitle(), '')
    driver.Quit()
    driver = self.CreateDriver(chrome_switches=[
                                   '--user-data-dir=%s' % temp_dir,
                               ])

  def testFindElementWhileNavigating(self):
    """A regression test for chromedriver:4459.
       The error manifests itself in flakiness rate >= 75%.
       This test creates a situation where a page keeps navigating
       while the client tries to find an element that will only appear
       after the last navigation. All such attempts to find an element
       must fail with NoSuchElement error. Other error codes,
       like NoSuchWindow, indicate a bug in ChromeDriver.
    """
    # iteration_count >= ceil(log(1 - 0.75) / log(1 - p))
    # where 'p' - probability of the iteration failure, found experimentally.
    iteration_count = 8
    for k in range(0, iteration_count):
      self._http_server.SetDataForPath('/page_%d.html' % k,
       bytes("""
       <html><body>
       <script>
         let index = %d;
         let url_base = "%s";
         let iteration_count = %d;
         if (index < iteration_count - 1) {
           let url = url_base + "/page_" + (index + 1) + ".html";
           setTimeout(()=>{location.assign(url)}, 300);
         } else {
           let div = document.createElement("div");
           div.id = "done";
           document.body.appendChild(div);
         }
       </script>
       </body></html>""" % (k, self._http_server.GetUrl(), iteration_count),
       'utf-8'))
    self._driver.Load(self.GetHttpUrlForFile('/page_0.html'))
    while True:
      try:
        self._driver.FindElement('css selector', '#done')
        break
      except chromedriver.NoSuchElement:
        pass

class ChromeDriverBackgroundTest(ChromeDriverBaseTestWithWebServer):
  def setUp(self):
    self._driver1 = self.CreateDriver()
    self._driver2 = self.CreateDriver()

  def testBackgroundScreenshot(self):
    self._driver2.Load(self._http_server.GetUrl('localhost')
                      + '/chromedriver/empty.html')
    self._driver1.Load(self._http_server.GetUrl('localhost')
                      + '/chromedriver/empty.html')

    screenshotPNGBase64  = self._driver1.TakeScreenshot()
    self.assertIsNotNone(screenshotPNGBase64)

# Tests that require a secure context.
class ChromeDriverSecureContextTest(ChromeDriverBaseTestWithWebServer):
  # The example attestation private key from the U2F spec at
  # https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-example
  # PKCS.8 encoded without encryption, as a base64url string.
  privateKey = ("MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg8_zMDQDYAxlU-Q"
                "hk1Dwkf0v18GZca1DMF3SaJ9HPdmShRANCAASNYX5lyVCOZLzFZzrIKmeZ2jwU"
                "RmgsJYxGP__fWN_S-j5sN4tT15XEpN_7QZnt14YvI6uvAgO0uJEboFaZlOEB")

  @staticmethod
  def GetHttpsUrlForFile(file_path, host=None):
    return ChromeDriverSecureContextTest._https_server.GetUrl(
        host) + file_path

  # Encodes a string in URL-safe base64 with no padding.
  @staticmethod
  def URLSafeBase64Encode(string):
    if isinstance(string, str):
      string = string.encode('utf-8')
    encoded = base64.urlsafe_b64encode(string)
    while encoded[-1] == b"=":
      encoded = encoded[0:-1]
    return encoded.decode('utf-8')

  # Decodes a base64 string with no padding.
  @staticmethod
  def UrlSafeBase64Decode(string):
    if isinstance(string, str):
      string = string.encode('utf-8')
    if len(string) % 4 != 0:
      string += b'=' * (4 - len(string) % 4)
    return base64.urlsafe_b64decode(string).decode('utf-8')

  def setUp(self):
    self._driver = self.CreateDriver(
        accept_insecure_certs=True,
        chrome_switches=['host-resolver-rules=MAP * 127.0.0.1',
            'enable-experimental-web-platform-features'])


  def testAddVirtualAuthenticator(self):
    def addAuthenticatorAndRegister(javascriptFragment, addArgs):
      script = """
        let done = arguments[0];
        registerCredential({
          authenticatorSelection: {
            requireResidentKey: true,
          },
          extensions: {""" + javascriptFragment + """
          },
        }).then(done);
      """
      self._driver.Load(self.GetHttpsUrlForFile(
          '/chromedriver/webauthn_test.html', 'chromedriver.test'))
      authenticatorId = self._driver.AddVirtualAuthenticator(
          protocol = 'ctap2_1',
          transport = 'usb',
          hasResidentKey = True,
          hasUserVerification = True,
          isUserConsenting = True,
          isUserVerified = True,
          **addArgs)
      result = self._driver.ExecuteAsyncScript(script)
      self._driver.RemoveVirtualAuthenticator(authenticatorId)
      return result

    with self.subTest(extension = 'largeBlob'):
      result = addAuthenticatorAndRegister(
          "largeBlob: { support: 'preferred' }",
          {'extensions': ['largeBlob']},
          )
      self.assertEqual('OK', result['status'])
      self.assertEqual(['usb'], result['credential']['transports'])
      self.assertEqual(True, result['extensions']['largeBlob']['supported'])

    with self.subTest(extension = 'minPinLength'):
      result = addAuthenticatorAndRegister(
          "minPinLength: true",
          {'extensions': ['minPinLength']},
          )
      self.assertEqual('OK', result['status'])
      authData = codecs.decode(
          bytes(result['credential']['authenticatorData'], 'ascii'), 'base64')
      self.assertTrue(b'minPinLength' in authData)

    with self.subTest(extension = 'credBlob'):
      result = addAuthenticatorAndRegister(
          "credBlob: new Uint8Array([1,2,3,4])",
          {'extensions': ['credBlob']},
          )
      self.assertEqual('OK', result['status'])
      authData = codecs.decode(
          bytes(result['credential']['authenticatorData'], 'ascii'), 'base64')
      # 0xf5 is 'true' in CBOR.
      self.assertTrue(b'credBlob\xf5' in authData)

    with self.subTest(extension = 'prf'):
      result = addAuthenticatorAndRegister(
          "prf: {}",
          {'extensions': ['prf']},
          )
      self.assertEqual('OK', result['status'])
      self.assertEqual(True, result['extensions']['prf']['enabled'])

  def testAddVirtualAuthenticatorProtocolVersion(self):
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))
    for protocol in ['ctap1/u2f', 'ctap2', 'ctap2_1']:
      authenticator_id = self._driver.AddVirtualAuthenticator(
          protocol = protocol,
          transport = 'usb',
      )
      self.assertTrue(len(authenticator_id) > 0)

    self.assertRaisesRegex(
        chromedriver.UnsupportedOperation,
        'INVALID is not a recognized protocol version',
        self._driver.AddVirtualAuthenticator,
            protocol = 'INVALID',
            transport = 'usb')

  def testAddVirtualBadExtensions(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'extensions must be a list of strings',
        self._driver.AddVirtualAuthenticator, protocol = 'ctap2', transport =
        'usb', extensions = 'invalid')

    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'extensions must be a list of strings',
        self._driver.AddVirtualAuthenticator, protocol = 'ctap2', transport =
        'usb', extensions = [42])

    self.assertRaisesRegex(
        chromedriver.UnsupportedOperation,
        'smolBlowbs is not a recognized extension',
        self._driver.AddVirtualAuthenticator, protocol = 'ctap2', transport =
        'usb', extensions = ['smolBlowbs'])

  def testAddVirtualAuthenticatorDefaultParams(self):
    script = """
      let done = arguments[0];
      registerCredential().then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))
    self._driver.AddVirtualAuthenticator(
        protocol = 'ctap1/u2f',
        transport = 'usb',
    )
    result = self._driver.ExecuteAsyncScript(script)
    self.assertEqual('OK', result['status'])
    self.assertEqual(['usb'], result['credential']['transports'])

  def testAddVirtualAuthDefaultBackupSettings(self):
    registerCredentialScript = """
      let done = arguments[0];
      registerCredential().then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))

    for backupState in [False, True]:
      for backupEligibility in [False, True]:
        # Add a virtual authenticator with the specified default backup flag
        # values.
        authenticatorId = self._driver.AddVirtualAuthenticator(
            protocol = 'ctap2',
            transport = 'usb',
            hasResidentKey = True,
            hasUserVerification = True,
            isUserVerified = True,
            defaultBackupState = backupState,
            defaultBackupEligibility = backupEligibility
        )

        # Creating a credential through the web API should reflect the default
        # values.
        result = self._driver.ExecuteAsyncScript(registerCredentialScript)
        self.assertEqual('OK', result['status'])
        self.assertEqual(backupEligibility, result['credential']['flags']['be'])
        self.assertEqual(backupState, result['credential']['flags']['bs'])

        # Getting the credential through webdriver should reflect the values.
        credentials = self._driver.GetCredentials(authenticatorId)
        self.assertEqual(1, len(credentials))
        self.assertEqual(backupEligibility, credentials[0]['backupEligibility'])
        self.assertEqual(backupState, credentials[0]['backupState'])

        # Cleanup.
        self._driver.RemoveVirtualAuthenticator(authenticatorId)

  def testRemoveVirtualAuthenticator(self):
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))

    # Removing a non existent virtual authenticator should fail.
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'Could not find a Virtual Authenticator matching the ID',
        self._driver.RemoveVirtualAuthenticator, 'id')

    # Create an authenticator and try removing it.
    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
        hasResidentKey = False,
        hasUserVerification = False,
    )
    self._driver.RemoveVirtualAuthenticator(authenticatorId)

    # Trying to remove the same authenticator should fail.
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'Could not find a Virtual Authenticator matching the ID',
        self._driver.RemoveVirtualAuthenticator, authenticatorId)

  def testAddCredential(self):
    script = """
      let done = arguments[0];
      getCredential({
        type: "public-key",
        id: new TextEncoder().encode("cred-1"),
        transports: ["usb"],
      }).then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))

    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
        hasResidentKey = True,
        hasUserVerification = False,
    )

    # Register a credential.
    self._driver.AddCredential(
      userHandle = self.URLSafeBase64Encode("marisa"),
      authenticatorId = authenticatorId,
      credentialId = self.URLSafeBase64Encode("cred-1"),
      isResidentCredential=True,
      rpId="chromedriver.test",
      privateKey=self.privateKey,
      signCount=1,
      userName="marisa",
      userDisplayName="Marisa Kirisame",
    )

    # Try authenticating with the credential.
    result = self._driver.ExecuteAsyncScript(script)
    self.assertEqual('OK', result['status'])

    # Verify the name and display name.
    credentials = self._driver.GetCredentials(authenticatorId)
    print(credentials)
    self.assertEqual(1, len(credentials))
    self.assertEqual("marisa", credentials[0]["userName"])
    self.assertEqual("Marisa Kirisame", credentials[0]["userDisplayName"])

  def testAddCredentialLargeBlob(self):
    script = """
      let done = arguments[0];
      getCredential({
        type: "public-key",
        id: new TextEncoder().encode("cred-1"),
        transports: ["usb"],
      }, {
        extensions: {
          largeBlob: {
            read: true,
          },
        },
      }).then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))

    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2_1',
        transport = 'usb',
        hasResidentKey = True,
        hasUserVerification = True,
        isUserVerified = True,
        extensions = ['largeBlob']
    )

    # Register a credential with a large blob and try reading it.
    self._driver.AddCredential(
      authenticatorId = authenticatorId,
      credentialId = self.URLSafeBase64Encode('cred-1'),
      userHandle = self.URLSafeBase64Encode('erina'),
      largeBlob = self.URLSafeBase64Encode('large blob contents'),
      isResidentCredential = True,
      rpId = "chromedriver.test",
      privateKey = self.privateKey,
      signCount = 1,
    )

    result = self._driver.ExecuteAsyncScript(script)
    self.assertEqual('OK', result['status'])
    self.assertEqual('large blob contents', result['blob'])

  def testAddCredentialBackupFlags(self):
    script = """
      let done = arguments[0];
      getCredential({
        type: "public-key",
        id: new TextEncoder().encode("cred-1"),
        transports: ["usb"],
      }).then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))

    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
        hasResidentKey = True,
        hasUserVerification = True,
        isUserVerified = True,
    )

    credentialId = self.URLSafeBase64Encode("cred-1")
    for backupState in [False, True]:
      for backupEligibility in [False, True]:
        # Create a credential with the given backup flags.
        self._driver.AddCredential(
          authenticatorId = authenticatorId,
          credentialId = credentialId,
          userHandle = self.URLSafeBase64Encode('melia'),
          isResidentCredential = True,
          rpId = "chromedriver.test",
          privateKey = self.privateKey,
          signCount = 1,
          backupState = backupState,
          backupEligibility = backupEligibility,
        )

        # Getting an assertion should reflect the values.
        result = self._driver.ExecuteAsyncScript(script)
        self.assertEqual('OK', result['status'])
        self.assertEqual(backupState, result['flags']['bs'])
        self.assertEqual(backupEligibility, result['flags']['be'])

        # Getting the credential through webdriver should reflect the values.
        credentials = self._driver.GetCredentials(authenticatorId)
        self.assertEqual(1, len(credentials))
        self.assertEqual(credentialId, credentials[0]['credentialId'])
        self.assertEqual(backupEligibility, credentials[0]['backupEligibility'])
        self.assertEqual(backupState, credentials[0]['backupState'])

        # Cleanup.
        self._driver.RemoveCredential(authenticatorId, credentialId)

  def testAddCredentialBase64Errors(self):
    # Test that AddCredential checks UrlBase64 parameteres.
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))

    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
        hasResidentKey = False,
        hasUserVerification = False,
    )

    # Try adding a credentialId that is encoded in vanilla base64.
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'credentialId must be a base64url encoded string',
        self._driver.AddCredential, authenticatorId, '_0n+wWqg=',
        False, "chromedriver.test", self.privateKey, None, 1,
    )

    # Try adding a credentialId that is not a string.
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'credentialId must be a base64url encoded string',
        self._driver.AddCredential, authenticatorId, 1,
        False, "chromedriver.test", self.privateKey, None, 1,
    )

  def testGetCredentials(self):
    script = """
      let done = arguments[0];
      registerCredential({
        authenticatorSelection: {
          requireResidentKey: true,
        },
        extensions: {
          largeBlob: {
            support: "required",
          },
        },
      }).then(attestation =>
          getCredential({
            type: "public-key",
            id: Uint8Array.from(attestation.credential.rawId),
            transports: ["usb"],
          }, {
            extensions: {
              largeBlob: {
                write: new TextEncoder().encode("large blob contents"),
              },
            },
          })).then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))
    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2_1',
        transport = 'usb',
        hasResidentKey = True,
        hasUserVerification = True,
        isUserVerified = True,
        extensions = ['largeBlob']
    )

    # Register a credential via the webauthn API and set a large blob on it.
    result = self._driver.ExecuteAsyncScript(script)
    self.assertEqual('OK', result['status'])
    self.assertEqual(True, result['extensions']['largeBlob']['written'])
    credentialId = result['attestation']['id']

    # GetCredentials should return the credential that was just created.
    credentials = self._driver.GetCredentials(authenticatorId)
    self.assertEqual(1, len(credentials))
    self.assertEqual(credentialId, credentials[0]['credentialId'])
    self.assertEqual(True, credentials[0]['isResidentCredential'])
    self.assertEqual('chromedriver.test', credentials[0]['rpId'])
    self.assertEqual(chr(1),
                      self.UrlSafeBase64Decode(credentials[0]['userHandle']))
    self.assertEqual(2, credentials[0]['signCount'])
    self.assertTrue(credentials[0]['privateKey'])
    self.assertEqual('large blob contents',
            self.UrlSafeBase64Decode(credentials[0]['largeBlob']))
    self.assertEqual('name', credentials[0]['userName'])
    self.assertEqual('displayName', credentials[0]['userDisplayName'])

  def testRemoveCredential(self):
    script = """
      let done = arguments[0];
      registerCredential().then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))
    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
    )

    # Register two credentials.
    result = self._driver.ExecuteAsyncScript(script)
    self.assertEqual('OK', result['status'])
    credential1Id = result['credential']['id']

    result = self._driver.ExecuteAsyncScript(script)
    self.assertEqual('OK', result['status'])
    credential2Id = result['credential']['id']

    # GetCredentials should return both credentials.
    credentials = self._driver.GetCredentials(authenticatorId)
    self.assertEqual(2, len(credentials))

    # Removing the first credential should leave only the first one.
    self._driver.RemoveCredential(authenticatorId, credential1Id)
    credentials = self._driver.GetCredentials(authenticatorId)
    self.assertEqual(1, len(credentials))
    self.assertEqual(credential2Id, credentials[0]['credentialId'])

  def testRemoveAllCredentials(self):
    register_credential_script = """
      let done = arguments[0];
      registerCredential().then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))
    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
    )

    # Register a credential via the webauthn API.
    result = self._driver.ExecuteAsyncScript(register_credential_script)
    self.assertEqual('OK', result['status'])
    credentialId = result['credential']['rawId']

    # Attempting to register with the credential ID on excludeCredentials should
    # fail.
    exclude_credentials_script = """
      let done = arguments[0];
      registerCredential({
        excludeCredentials: [{
          type: "public-key",
          id: Uint8Array.from(%s),
          transports: ["usb"],
        }],
      }).then(done);
    """ % (credentialId)
    result = self._driver.ExecuteAsyncScript(exclude_credentials_script)
    self.assertEqual("InvalidStateError: The user attempted to register an "
                      "authenticator that contains one of the credentials "
                      "already registered with the relying party.",
                      result['status'])

    # The registration should succeed after clearing the credentials.
    self._driver.RemoveAllCredentials(authenticatorId)
    result = self._driver.ExecuteAsyncScript(exclude_credentials_script)
    self.assertEqual('OK', result['status'])

  def testSetUserVerified(self):
    register_uv_script = """
      let done = arguments[0];
      registerCredential({
        authenticatorSelection: {
          userVerification: "required",
        },
      }).then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))
    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
        hasResidentKey = True,
        hasUserVerification = True,
    )

    # Configure the virtual authenticator to fail user verification.
    self._driver.SetUserVerified(authenticatorId, False)

    # Attempting to register a credential with UV required should fail.
    result = self._driver.ExecuteAsyncScript(register_uv_script)
    self.assertTrue(result['status'].startswith("NotAllowedError"),
                    "Expected %s to be a NotAllowedError" % (result['status']))

    # Trying again after setting userVerified to True should succeed.
    self._driver.SetUserVerified(authenticatorId, True)
    result = self._driver.ExecuteAsyncScript(register_uv_script)
    self.assertEqual("OK", result['status'])

  def testSetCredentialProperties(self):
    script = """
      let done = arguments[0];
      getCredential({
        type: "public-key",
        id: new TextEncoder().encode("cred-1"),
        transports: ["usb"],
      }).then(done);
    """
    self._driver.Load(self.GetHttpsUrlForFile(
        '/chromedriver/webauthn_test.html', 'chromedriver.test'))

    authenticatorId = self._driver.AddVirtualAuthenticator(
        protocol = 'ctap2',
        transport = 'usb',
        hasResidentKey = True,
        hasUserVerification = True,
        isUserVerified = True,
    )
    credentialId = self.URLSafeBase64Encode("cred-1")

    # Create a credential with default backup flags.
    self._driver.AddCredential(
      authenticatorId = authenticatorId,
      credentialId = credentialId,
      userHandle = self.URLSafeBase64Encode('melia'),
      isResidentCredential = True,
      rpId = "chromedriver.test",
      privateKey = self.privateKey,
      signCount = 1,
    )
    for backupState in [False, True]:
      for backupEligibility in [False, True]:
        # Set the credential properties.
        self._driver.SetCredentialProperties(
            authenticatorId = authenticatorId, credentialId = credentialId,
            backupState = backupState, backupEligibility = backupEligibility)

        # Getting an assertion should reflect the values.
        result = self._driver.ExecuteAsyncScript(script)
        self.assertEqual('OK', result['status'])
        self.assertEqual(backupState, result['flags']['bs'])
        self.assertEqual(backupEligibility, result['flags']['be'])

        # Getting the credential through webdriver should reflect the values.
        credentials = self._driver.GetCredentials(authenticatorId)
        self.assertEqual(1, len(credentials))
        self.assertEqual(credentialId, credentials[0]['credentialId'])
        self.assertEqual(backupEligibility, credentials[0]['backupEligibility'])
        self.assertEqual(backupState, credentials[0]['backupState'])

  def testCreateVirtualSensorWithInvalidSensorName(self):
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           "invalid argument: 'type' must be a string",
                           self._driver.CreateVirtualSensor, 42)

  def testUpdateVirtualSensorWithInvalidSensorName(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "invalid argument: Unexpected type invalid_sensor in 'type' field",
        self._driver.UpdateVirtualSensor, 'invalid_sensor', {
            'x': 1.0,
            'y': 2.0,
            'z': 3.0
        })

  def testUpdateVirtualSensorWithoutIlluminanceValue(self):
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           "invalid argument: Could not parse illuminance",
                           self._driver.UpdateVirtualSensor, 'ambient-light',
                           {'invalid_value_key': 1.0})

  def testUpdateVirtualSensorWithoutXYZValues(self):
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           "invalid argument: Could not parse XYZ fields",
                           self._driver.UpdateVirtualSensor, 'accelerometer', {
                               'y': 2.0,
                               'z': 3.0
                           })
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           "invalid argument: Could not parse XYZ fields",
                           self._driver.UpdateVirtualSensor, 'accelerometer', {
                               'x': 1.0,
                               'z': 3.0
                           })
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           "invalid argument: Could not parse XYZ fields",
                           self._driver.UpdateVirtualSensor, 'accelerometer', {
                               'x': 1.0,
                               'y': 2.0
                           })

  def testUpdateVirtualSensorWithoutXYZWValues(self):
    expected_error = ("invalid argument: Could not parse absolute-orientation "
                      "readings. Invalid alpha/beta/gamma values")
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           expected_error,
                           self._driver.UpdateVirtualSensor,
                           'absolute-orientation', {
                               'y': 2.0,
                               'z': 3.0,
                               'w': 4.0
                           })
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           expected_error,
                           self._driver.UpdateVirtualSensor,
                           'absolute-orientation', {
                               'x': 1.0,
                               'z': 3.0,
                               'w': 4.0
                           })
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           expected_error,
                           self._driver.UpdateVirtualSensor,
                           'absolute-orientation', {
                               'x': 1.0,
                               'y': 2.0,
                               'w': 4.0
                           })
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           expected_error,
                           self._driver.UpdateVirtualSensor,
                           'absolute-orientation', {
                               'x': 1.0,
                               'y': 2.0,
                               'z': 3.0
                           })

  def testUpdateVirtualSensorWithoutAlphaBetaGammaValues(self):
    expected_error = ("invalid argument: Could not parse relative-orientation "
                      "readings. Invalid alpha/beta/gamma values")
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           expected_error,
                           self._driver.UpdateVirtualSensor,
                           'relative-orientation', {
                               'beta': 2.0,
                               'gamma': 3.0
                           })
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           expected_error,
                           self._driver.UpdateVirtualSensor,
                           'relative-orientation', {
                               'alpha': 1.0,
                               'gamma': 3.0
                           })
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           expected_error,
                           self._driver.UpdateVirtualSensor,
                           'relative-orientation', {
                               'alpha': 1.0,
                               'beta': 2.0
                           })

  def testUpdateVirtualSensorOutOfRangeEulerAngles(self):
    # Alpha, beta and gamma must be within the ranges defined by the Device
    # Orientation API.
    test_inputs = (
      # Alpha range: [0, 360).
      [-1, 2, 3], [361, 2, 3],
      # Beta range: [-180, 180).
      [1, -181, 3], [1, 180, 3],
      # Gamma range: [-90, 90).
      [1, 2, -91], [1, 2, 90]
    )
    expected_error = ("invalid argument: Could not parse relative-orientation "
                      "readings. Invalid alpha/beta/gamma values")
    for test_input in test_inputs:
      alpha, beta, gamma = test_input
      self.assertRaisesRegex(chromedriver.InvalidArgument,
                            expected_error,
                            self._driver.UpdateVirtualSensor,
                            'relative-orientation',
                            { 'alpha': alpha, 'beta': beta, 'gamma': gamma })

  def testRemoveVirtualSensorWithInvalidSensorName(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "invalid argument: Invalid sensor type: invalid_sensor",
        self._driver.RemoveVirtualSensor, 'invalid_sensor')

  def testGetVirtualSensorInformationWithInvalidSensorName(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "invalid argument: Invalid sensor type: invalid_sensor",
        self._driver.GetVirtualSensorInformation, 'invalid_sensor')

  def testGetVirtualSensorInformationWithNotOverriddenType(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "This sensor type is not being overridden with a virtual sensor",
        self._driver.GetVirtualSensorInformation, 'accelerometer')

  def testCreateAlreadyOverriddenVirtualSensor(self):
    self._driver.CreateVirtualSensor('accelerometer')
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/sensors_test.html'))
    self.assertRaisesRegex(chromedriver.InvalidArgument,
                           'The specified sensor type is already overridden',
                           self._driver.CreateVirtualSensor, 'accelerometer')

  def testCreateVirtualSensorNotConnected(self):
    self._driver.CreateVirtualSensor('accelerometer', {'connected': False})
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/sensors_test.html'))

    self._driver.SetPermission({
      'descriptor': { 'name': 'accelerometer' },
      'state': 'granted'
    })

    self._driver.ExecuteScript('startSensor()')
    self.assertTrue(
        self.WaitForCondition(lambda: self._driver.ExecuteScript(
            'return sensorErrorEvent !== undefined')))

    error_name = self._driver.ExecuteScript(
        'return sensorErrorEvent.error.name')
    self.assertEqual('NotReadableError', error_name)


  def testCreateVirtualSensorWithMinimumFrequency(self):
    self._driver.CreateVirtualSensor('accelerometer',
                                     {'minSamplingFrequency': 6})
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/sensors_test.html'))

    self._driver.SetPermission({
      'descriptor': { 'name': 'accelerometer' },
      'state': 'granted'
    })

    self._driver.ExecuteScript('startSensor()')
    self.assertTrue(
        self.WaitForCondition(
            lambda: self._driver.ExecuteScript('return sensor.activated')))

    info = self._driver.GetVirtualSensorInformation('accelerometer')
    self.assertEqual(6.0, info['requestedSamplingFrequency'])

  def testCreateVirtualSensorWithMaximumFrequency(self):
    self._driver.CreateVirtualSensor('accelerometer',
                                     {'maxSamplingFrequency': 3})
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/sensors_test.html'))

    self._driver.SetPermission({
      'descriptor': { 'name': 'accelerometer' },
      'state': 'granted'
    })

    self._driver.ExecuteScript('startSensor()')
    self.assertTrue(
        self.WaitForCondition(
            lambda: self._driver.ExecuteScript('return sensor.activated')))

    info = self._driver.GetVirtualSensorInformation('accelerometer')
    self.assertEqual(3.0, info['requestedSamplingFrequency'])

  def testGetVirtualSensorInformation(self):
    self._driver.CreateVirtualSensor('accelerometer')
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/sensors_test.html'))

    self._driver.SetPermission({
      'descriptor': { 'name': 'accelerometer' },
      'state': 'granted'
    })

    info = self._driver.GetVirtualSensorInformation('accelerometer')
    self.assertEqual(0.0, info['requestedSamplingFrequency'])

    self._driver.ExecuteScript('startSensor()')
    self.assertTrue(
        self.WaitForCondition(
            lambda: self._driver.ExecuteScript('return sensor.activated')))

    info = self._driver.GetVirtualSensorInformation('accelerometer')
    self.assertLess(0.0, info['requestedSamplingFrequency'])

  def testUpdateVirtualSensor(self):
    testedSensor = 'accelerometer'

    self._driver.CreateVirtualSensor(testedSensor)
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/sensors_test.html'))

    self._driver.SetPermission({
      'descriptor': { 'name': 'accelerometer' },
      'state': 'granted'
    })

    self._driver.ExecuteScript('startSensor()')

    reading = {'x': 1.0, 'y': 2.0, 'z': 3.0}

    self._driver.UpdateVirtualSensor(testedSensor, reading)

    self.assertTrue(
        self.WaitForCondition(
            lambda: self._driver.ExecuteScript('return sensor.hasReading')))

    sensor = self._driver.ExecuteScript('return sensor')
    self.assertEqual(reading['x'], sensor['x'])
    self.assertEqual(reading['y'], sensor['y'])
    self.assertEqual(reading['z'], sensor['z'])

    self._driver.RemoveVirtualSensor(testedSensor)

  def testUpdateVirtualSensorWitNotOverriddenType(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "This sensor type is not being overridden with a virtual sensor",
        self._driver.UpdateVirtualSensor, 'ambient-light', {'illuminance': 42})

  def testSetDevicePosture(self):
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/device_posture_test.html'))
    self._driver.ExecuteScript('addDevicePostureEventListener()')
    original_posture = self._driver.ExecuteScript(
        'return navigator.devicePosture.type')
    posture = 'folded' if original_posture == 'continuous' else 'continuous'
    self._driver.SetDevicePosture(posture)
    self.assertTrue(
        self.WaitForCondition(lambda: self._driver.ExecuteScript(
            'return postures.length === 1')))
    self.assertNotEqual(original_posture,
                        self._driver.ExecuteScript('return postures.at(-1)'))
    self._driver.SetDevicePosture(original_posture)
    self.assertTrue(
        self.WaitForCondition(lambda: self._driver.ExecuteScript(
            'return postures.length === 2')))
    self.assertEqual(original_posture,
                     self._driver.ExecuteScript('return postures.at(-1)'))

  def testSetDevicePostureInvalidArgument(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "Invalid posture type",
        self._driver.SetDevicePosture, 'invalid-posture')

  def testClearDevicePosture(self):
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/device_posture_test.html'))
    self._driver.ExecuteScript('addDevicePostureEventListener()')
    original_posture = self._driver.ExecuteScript(
        'return navigator.devicePosture.type')
    posture = 'folded' if original_posture == 'continuous' else 'continuous'
    self._driver.SetDevicePosture(posture)
    self.assertTrue(
        self.WaitForCondition(lambda: self._driver.ExecuteScript(
            'return postures.length === 1')))
    self.assertNotEqual(original_posture,
                        self._driver.ExecuteScript('return postures.at(-1)'))
    self._driver.ClearDevicePosture()
    self.assertTrue(
        self.WaitForCondition(lambda: self._driver.ExecuteScript(
            'return postures.length === 2')))
    self.assertEqual(original_posture,
                     self._driver.ExecuteScript('return postures.at(-1)'))

  def testClearDevicePostureWithoutSetDevicePosture(self):
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/device_posture_test.html'))
    self._driver.ExecuteScript('addDevicePostureEventListener()')
    self._driver.ClearDevicePosture()
    self.assertFalse(
        self.WaitForCondition(lambda: self._driver.ExecuteScript(
            'return postures.length === 1')))

  def testCreateVirtualPressureSourceNotConnected(self):
    script = """
      const done = arguments[0];
      const observer = new PressureObserver(() => {
        throw Error("The observer callback should not be called");
      });
      observer.observe("cpu").then(() => {
        throw new Error('Observation did not fail as expected');
      }, e => {
        done(e.name);
      });
    """
    self._driver.CreateVirtualPressureSource('cpu', {'supported': False})
    self._driver.Load(
        self.GetHttpsUrlForFile('/chromedriver/compute_pressure_test.html'))
    self.assertEqual('NotSupportedError',
                     self._driver.ExecuteAsyncScript(script))


# Tests in the following class are expected to be moved to ChromeDriverTest
# class when we no longer support the legacy mode.
class ChromeDriverW3cTest(ChromeDriverBaseTestWithWebServer):
  """W3C mode specific tests."""

  def setUp(self):
    self._driver = self.CreateDriver(
        send_w3c_capability=True, send_w3c_request=True)

  def testSendKeysToElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    text = self._driver.ExecuteScript(
        'document.body.innerHTML = \'<input type="text">\';'
        'var input = document.getElementsByTagName("input")[0];'
        'input.addEventListener("change", function() {'
        '  document.body.appendChild(document.createElement("br"));'
        '});'
        'return input;')
    text.SendKeys('0123456789+-*/ Hi')
    text.SendKeys(', there!')
    value = self._driver.ExecuteScript('return arguments[0].value;', text)
    self.assertEqual('0123456789+-*/ Hi, there!', value)

  def testSendKeysToElementDoesNotAppend(self):
      self._driver.Load(self.GetHttpUrlForFile(
          '/chromedriver/empty.html'))
      textControlTypes = ["text", "search", "tel", "url",  "password"]
      for textType in textControlTypes:
          element = self._driver.ExecuteScript(
              'document.body.innerHTML = '
              '\'<input type="{}" value="send_this_value">\';'
              'var input = document.getElementsByTagName("input")[0];'
              'input.focus();'
              'input.setSelectionRange(0,0);'
              'return input;'.format(textType))
          element.SendKeys('hello')
          value = self._driver.ExecuteScript('return arguments[0].value;',
                                             element)
          self.assertEqual('hellosend_this_value', value)

  def testSendKeysToEditableElement(self):
      self._driver.Load(self.GetHttpUrlForFile(
          '/chromedriver/empty.html'))
      element = self._driver.ExecuteScript(
          'document.body.innerHTML = '
          '\'<p contentEditable="true"> <i>hello-></i> '
          '<b>send_this_value </b> </p>\';'
          'var input = document.getElementsByTagName("i")[0];'
          'return input;')
      element.SendKeys('hello')
      self.assertEqual('hello->hello', element.GetText())

      self._driver.Load(self.GetHttpUrlForFile(
          '/chromedriver/empty.html'))
      element = self._driver.ExecuteScript(
          'document.body.innerHTML = '
          '\'<p contentEditable="true"> <i>hello</i> '
          '<b>-></b> </p>\';'
          'var input = document.getElementsByTagName("p")[0];'
          'input.focus();'
          'return input;')
      element.SendKeys('hello')
      self.assertEqual('hellohello ->', element.GetText())

  def testUnexpectedAlertOpenExceptionMessage(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript('window.alert("Hi");')
    self.assertRaisesRegex(chromedriver.UnexpectedAlertOpen,
                            '{Alert text : Hi}',
                            self._driver.FindElement, 'tag name', 'divine')
    # In W3C mode, the alert is dismissed by default.
    self.assertFalse(self._driver.IsAlertOpen())


class ChromeDriverTestLegacy(ChromeDriverBaseTestWithWebServer):
  """End to end tests for ChromeDriver in Legacy mode."""

  def setUp(self):
    self._driver = self.CreateDriver(send_w3c_capability=False,
                                     send_w3c_request=False)

  def testContextMenuEventFired(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/context_menu.html'))
    self._driver.MouseMoveTo(self._driver.FindElement('tag name', 'div'))
    self._driver.MouseClick(2)
    self.assertTrue(self._driver.ExecuteScript('return success'))

  def testDragAndDropWithSVGImage(self):
    self._driver.Load(
        self.GetHttpUrlForFile('/chromedriver/drag_and_drop.svg'))
    drag = self._driver.FindElement("css selector", "#GreenRectangle")
    drop = self._driver.FindElement("css selector", "#FolderRectangle")
    self._driver.MouseMoveTo(drag)
    self._driver.MouseButtonDown()
    self._driver.MouseMoveTo(drop)
    self._driver.MouseButtonUp()
    self.assertTrue(self._driver.IsAlertOpen())
    self.assertEqual('GreenRectangle has been dropped into a folder.',
                      self._driver.GetAlertMessage())
    self._driver.HandleAlert(True)
    self.assertEqual('translate(300,55)', drag.GetAttribute("transform"))

  def testMouseButtonDownAndUp(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("mousedown", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new1<br>";'
        '});'
        'div.addEventListener("mouseup", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new2<a></a>";'
        '});')
    self._driver.MouseMoveTo(None, 50, 50)
    self._driver.MouseButtonDown()
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))
    self._driver.MouseButtonUp()
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'a')))

  def testMouseClick(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("click", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    self._driver.MouseMoveTo(div)
    self._driver.MouseClick()
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testMouseDoubleClick(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("dblclick", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    self._driver.MouseMoveTo(div, 1, 1)
    self._driver.MouseDoubleClick()
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testMouseMoveTo(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.style["width"] = "100px";'
        'div.style["height"] = "100px";'
        'div.addEventListener("mouseover", function() {'
        '  var div = document.getElementsByTagName("div")[0];'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    self._driver.MouseMoveTo(div, 10, 10)
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testMoveToElementAndClick(self):
    # This page gets rendered differently depending on which platform the test
    # is running on, and what window size is being used. So we need to do some
    # sanity checks to make sure that the <a> element is split across two lines
    # of text.
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/multiline.html'))

    # Check that link element spans two lines and that the first ClientRect is
    # above the second.
    link = self._driver.FindElements('tag name', 'a')[0]
    client_rects = self._driver.ExecuteScript(
        'return arguments[0].getClientRects();', link)
    self.assertEqual(2, len(client_rects))
    self.assertTrue(client_rects[0]['bottom'] <= client_rects[1]['top'])

    # Check that the center of the link's bounding ClientRect is outside the
    # element.
    bounding_client_rect = self._driver.ExecuteScript(
        'return arguments[0].getBoundingClientRect();', link)
    center = bounding_client_rect['left'] + bounding_client_rect['width'] / 2
    self.assertTrue(client_rects[1]['right'] < center)
    self.assertTrue(center < client_rects[0]['left'])

    self._driver.MouseMoveTo(link)
    self._driver.MouseClick()
    self.assertTrue(self._driver.GetCurrentUrl().endswith('#top'))


  def _FindElementInShadowDom(self, css_selectors):
    """Find an element inside shadow DOM using CSS selectors.
    The last item in css_selectors identify the element to find. All preceding
    selectors identify the hierarchy of shadow hosts to traverse in order to
    reach the target shadow DOM."""
    current = None
    for selector in css_selectors:
      if current is None:
        # First CSS selector, start from root DOM.
        current = self._driver
      else:
        # current is a shadow host selected previously.
        # Enter the corresponding shadow root.
        current = self._driver.ExecuteScript(
            'return arguments[0].shadowRoot', current)
      current = current.FindElement('css selector', selector)
    return current

  def testShadowDomDisplayed(self):
    """Checks that trying to manipulate shadow DOM elements that are detached
    from the document raises a StaleElementReference exception"""
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/shadow_dom_test.html'))
    elem = self._FindElementInShadowDom(
        ["#innerDiv", "#parentDiv", "#button"])
    self.assertTrue(elem.IsDisplayed())
    elem2 = self._driver.FindElement("css selector", "#hostContent")
    self.assertTrue(elem2.IsDisplayed())
    self._driver.ExecuteScript(
        'document.querySelector("#outerDiv").style.display="None";')
    self.assertFalse(elem.IsDisplayed())

  def testSendingTabKeyMovesToNextInputElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/two_inputs.html'))
    first = self._driver.FindElement('css selector', '#first')
    second = self._driver.FindElement('css selector', '#second')
    first.Click()
    self._driver.SendKeys('snoopy')
    self._driver.SendKeys('\uE004')
    self._driver.SendKeys('prickly pete')
    self.assertEqual('snoopy', self._driver.ExecuteScript(
        'return arguments[0].value;', first))
    self.assertEqual('prickly pete', self._driver.ExecuteScript(
        'return arguments[0].value;', second))

  def testMobileEmulationDisabledByDefault(self):
    self.assertFalse(self._driver.capabilities['mobileEmulationEnabled'])

  def testSendKeysToElement(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    text = self._driver.ExecuteScript(
        'document.body.innerHTML = \'<input type="text">\';'
        'var input = document.getElementsByTagName("input")[0];'
        'input.addEventListener("change", function() {'
        '  document.body.appendChild(document.createElement("br"));'
        '});'
        'return input;')
    text.SendKeys('0123456789+-*/ Hi')
    text.SendKeys(', there!')
    value = self._driver.ExecuteScript('return arguments[0].value;', text)
    self.assertEqual('0123456789+-*/ Hi, there!', value)

  def testUnexpectedAlertOpenExceptionMessage(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript('window.alert("Hi");')
    self.assertRaisesRegex(chromedriver.UnexpectedAlertOpen,
                            'unexpected alert open: {Alert text : Hi}',
                            self._driver.FindElement, 'tag name', 'divine')

  def testTouchScrollElement(self):
    self._driver.Load(self.GetHttpUrlForFile(
          '/chromedriver/touch_action_tests.html'))
    scroll_left = 'return document.documentElement.scrollLeft;'
    scroll_top = 'return document.documentElement.scrollTop;'
    self.assertEqual(0, self._driver.ExecuteScript(scroll_left))
    self.assertEqual(0, self._driver.ExecuteScript(scroll_top))
    target = self._driver.FindElement('css selector', '#target')
    self._driver.TouchScroll(target, 47, 53)
    # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1179
    self.assertAlmostEqual(47, self._driver.ExecuteScript(scroll_left), delta=1)
    self.assertAlmostEqual(53, self._driver.ExecuteScript(scroll_top), delta=1)

  def testTouchDoubleTapElement(self):
    self._driver.Load(self.GetHttpUrlForFile(
          '/chromedriver/touch_action_tests.html'))
    target = self._driver.FindElement('css selector', '#target')
    target.DoubleTap()
    events = self._driver.FindElement('css selector', '#events')
    self.assertEqual('events: touchstart touchend touchstart touchend',
                        events.GetText())

  def testTouchLongPressElement(self):
    self._driver.Load(self.GetHttpUrlForFile(
          '/chromedriver/touch_action_tests.html'))
    target = self._driver.FindElement('css selector', '#target')
    target.LongPress()
    events = self._driver.FindElement('css selector', '#events')
    self.assertEqual('events: touchstart touchcancel', events.GetText())

  def testTouchSingleTapElement(self):
    self._driver.Load(self.GetHttpUrlForFile(
          '/chromedriver/touch_action_tests.html'))
    target = self._driver.FindElement('css selector', '#target')
    target.SingleTap()
    events = self._driver.FindElement('css selector', '#events')
    self.assertEqual('events: touchstart touchend', events.GetText())

class ChromeDriverFencedFrame(ChromeDriverBaseTestWithWebServer):
  def setUp(self):
    super().setUp()
    self._https_server.SetDataForPath('/main.html', bytes("""
      <!DOCTYPE html>
        <html>
          <body>
            <fencedframe></fencedframe>
            <script>
              const url = new URL("fencedframe.html", location.href);
              document.querySelector("fencedframe").config =
                  new FencedFrameConfig(url);
            </script>
          </body>
        </html>
      """, 'utf-8'))

    self._https_server.SetDataForPath('/nesting.html', bytes("""
      <!DOCTYPE html>
        <html>
          <body>
            <iframe src="/main.html"></iframe>
          </body>
        </html>
    """, 'utf-8'))

    def respondWithFencedFrameContents(request):
      return {'Supports-Loading-Mode': 'fenced-frame'}, bytes("""
        <!DOCTYPE html>
        <html>
          <body>
            <button></button>
          </body>
        </html>""", 'utf-8')
    self._https_server.SetCallbackForPath('/fencedframe.html',
                                          respondWithFencedFrameContents)

    self._driver = self.CreateDriver(
        accept_insecure_certs = True,
        chrome_switches=['--site-per-process',
          '--enable-features=FencedFrames,PrivacySandboxAdsAPIsOverride,'
          'FencedFramesAPIChanges,FencedFramesDefaultMode,'
          'FencedFramesEnforceFocus'])

  @staticmethod
  def GetHttpsUrlForFile(file_path):
    return ChromeDriverFencedFrame._https_server.GetUrl() + file_path

  def tearDown(self):
    super().tearDown()
    self._https_server.SetDataForPath('/main.html', None)
    self._https_server.SetDataForPath('/nesting.html', None)
    self._https_server.SetCallbackForPath('/fencedframe.html', None)

  def testCanSwitchToFencedFrame(self):
    self._driver.Load(self.GetHttpsUrlForFile('/main.html'))
    self._driver.SetTimeouts({'implicit': 2000})
    fencedframe = self._driver.FindElement('tag name', 'fencedframe')
    self._driver.SwitchToFrame(fencedframe)
    button = self._driver.FindElement('tag name', 'button')
    self.assertIsNotNone(button)

  def testAppendEmptyFencedFrame(self):
    self._driver.Load(self.GetHttpsUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript(
        'document.body.appendChild(document.createElement("fencedframe"));')
    fencedframe = self._driver.FindElement('tag name', 'fencedframe')
    self.assertIsNotNone(fencedframe)
    self._driver.SwitchToFrame(fencedframe)

  def testFencedFrameInsideIframe(self):
    self._driver.Load(self.GetHttpsUrlForFile('/nesting.html'))
    self._driver.SwitchToFrameByIndex(0)
    fencedframe = self._driver.FindElement('tag name', 'fencedframe')
    self.assertIsNotNone(fencedframe)
    self._driver.SwitchToFrame(fencedframe)

  def testSharedStorageWorkletTarget(self):
    self._http_server.SetDataForPath('/simple_module.js', bytes('''
       class Simple {
          async run(urls, data) {
            return 0;
          }
       }
       register('simple', Simple);
    ''', 'utf-8'))

    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/empty.html'))
    self._driver.ExecuteScript('''
       window.sharedStorage.worklet.addModule(
                               "/shared-storage/resources/simple-module.js")''')
    window_handles = self._driver.GetWindowHandles()
    self.assertEqual(len(window_handles), 1)

class ChromeDriverSiteIsolation(ChromeDriverBaseTestWithWebServer):
  """Tests for ChromeDriver with the new Site Isolation Chrome feature.

  This feature can be turned on using the --site-per-process flag.

  In order to trick the test into thinking that we are on two separate origins,
  the cross_domain_iframe.html code points to localhost instead of 127.0.0.1.

  Note that Chrome does not allow "localhost" to be passed to --isolate-origins
  for fixable technical reasons related to subdomain matching.
  """

  def setUp(self):
    self._driver = self.CreateDriver(chrome_switches=['--site-per-process'])

  def testCanClickOOPIF(self):
    """Test that you can click into an Out of Process I-Frame (OOPIF).

    Note that the Iframe will not be out-of-process if the correct
    flags are not passed into Chrome.
    """
    if util.GetPlatformName() == 'win':
      # https://bugs.chromium.org/p/chromedriver/issues/detail?id=2198
      # This test is unreliable on Windows, as FindElement can be called too
      # soon, before the child frame is fully loaded. This causes element not
      # found error. Add an implicit wait works around this issue.
      self._driver.SetTimeouts({'implicit': 2000})
    self._driver.Load(self.GetHttpUrlForFile(
        '/chromedriver/cross_domain_iframe.html'))
    frame = self._driver.FindElement('tag name', 'iframe')
    self._driver.SwitchToFrame(frame)
    self.assertTrue(self.WaitForCondition(
        lambda: 'outer.html' in
                self._driver.ExecuteScript('return window.location.href')))
    self.assertTrue(self.WaitForCondition(
        lambda: 'complete' ==
                self._driver.ExecuteScript('return document.readyState')))
    self._driver.SwitchToMainFrame()
    a_outer = self._driver.FindElement('tag name', 'a')
    a_outer.Click()
    frame_url = self._driver.ExecuteScript('return window.location.href')
    self.assertTrue(frame_url.endswith('#one'))
    self._driver.SwitchToFrame(frame)
    a_inner = self._driver.FindElement('tag name', 'a')
    a_inner.Click()
    frame_url = self._driver.ExecuteScript('return window.location.href')
    self.assertTrue(frame_url.endswith('#two'))

  def testScriptNavigateLocalToLocal(self):
    """Test that user can switch into a local frame
    and perform a same domain navigation.
    """
    local2_url = self.GetHttpUrlForFile('/local2.html')
    self._http_server.SetDataForPath('/local2.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/local1.html',
      bytes('<p>Ready, Steady, Go!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="/local1.html">', 'utf-8'))
    for _ in range(0, 2):
      self._driver.SwitchToMainFrame()
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1)
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self._driver.ExecuteScript('location.href=arguments[0]', local2_url)
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1)
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testScriptNavigateLocalToRemote(self):
    """Test that user can switch into a local frame
    and perform a cross domain navigation.
    """
    remote_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote.html'),
                                      'localhost')
    self._http_server.SetDataForPath('/remote.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/local.html',
      bytes('<p>Ready, Steady, Go!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="/local.html">', 'utf-8'))
    for _ in range(0, 2):
      self._driver.SwitchToMainFrame()
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1)
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self._driver.ExecuteScript('location.href=arguments[0]', remote_url)
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1)
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testScriptNavigateRemoteToSameRemote(self):
    """Test that user can switch into a remote frame,
    belonging to a different domain than the main frame,
    and perform a same domain navigation.
    """
    remote1_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote1.html'),
                                      'localhost')
    remote2_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote2.html'),
                                      'localhost')
    self._http_server.SetDataForPath('/remote2.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/remote1.html',
      bytes('<p>Ready, Steady, Go!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="%s">' % remote1_url, 'utf-8'))
    for _ in range(0, 2):
      self._driver.SwitchToMainFrame()
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1)
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self._driver.ExecuteScript('location.href=arguments[0]', remote2_url)
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1)
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testScriptNavigateRemoteToLocal(self):
    """Test that user can switch into a remote frame,
    belonging to a different domain than the main frame,
    and perform a cross domain navigation to the main frame domain.

    The test reproduces a real bug that has some degree of flakiness.
    Therefore it has several iterations to increase the flakiness
    to the observable level.
    """
    local_url = self.GetHttpUrlForFile('/local.html')
    remote_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote.html'),
                                      'localhost')
    self._http_server.SetDataForPath('/local.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/remote.html',
      bytes('<p>Ready, Steady, Go!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="%s">' % remote_url, 'utf-8'))
    # The first iteration after session creation frequently succeeds.
    # Therefore we make two iterations.
    for _ in range(0, 2):
      self._driver.SwitchToMainFrame()
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1)
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self._driver.ExecuteScript('location.href=arguments[0]', local_url)
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1)
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testClickNavigateLocalToLocal(self):
    """Test that user can switch into a local frame
    and perform a same domain navigation by mouse click.
    """
    self._http_server.SetDataForPath('/local2.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/local1.html',
      bytes('<a href="local2.html">To Another Local</a>', 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="/local1.html">', 'utf-8'))
    for _ in range(0, 2):
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1)
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'a')) > 0,
        timeout=1)
      anchor = self._driver.FindElement('tag name', 'a')
      anchor.Click()
      self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1)
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testClickNavigateLocalToRemote(self):
    """Test that user can switch into a local frame
    and perform a cross domain navigation by mouse click.
    """
    remote_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote.html'),
                                      'localhost')
    self._http_server.SetDataForPath('/remote.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/local.html',
      bytes('<a href="%s">To Remote</a>' % remote_url, 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="/local.html">', 'utf-8'))
    for _ in range(0, 2):
      self._driver.SwitchToMainFrame()
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1))
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'a')) > 0,
        timeout=1))
      anchor = self._driver.FindElement('tag name', 'a')
      anchor.Click()
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1))
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testClickNavigateRemoteToSameRemote(self):
    """Test that user can switch into a remote frame,
    belonging to a different domain than the main frame,
    and perform a same domain navigation by mouse click.

    The test reproduces a real bug that has some degree of flakiness.
    Therefore it has several iterations to increase the flakiness
    to the observable level.
    """
    remote1_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote1.html'),
                                      'localhost')
    remote2_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote2.html'),
                                      'localhost')
    self._http_server.SetDataForPath('/remote2.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/remote1.html',
      bytes('<a href="%s">To Another Remote</a>' % remote2_url, 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="%s">' % remote1_url, 'utf-8'))
    # The bug manifests itself in 40% failure rate.
    # Three iterations bring it above 75%.
    # An extra iteration to heat up after the session creation.
    for _ in range(0, 4):
      self._driver.SwitchToMainFrame()
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1))
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self.assertTrue(self.WaitForCondition(
              lambda: len(self._driver.FindElements('tag name', 'a')) > 0,
              timeout=1))
      anchor = self._driver.FindElement('tag name', 'a')
      anchor.Click()
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1))
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testClickNavigateRemoteToLocal(self):
    """Test that user can switch into a remote frame, belonging to a different
    domain than the main frame, and perform a cross domain navigation
    to the main frame domain by mouse click.

    The test reproduces a real bug that has some degree of flakiness.
    Therefore it has several iterations to increase the flakiness
    to the observable level.
    """
    local_url = self.GetHttpUrlForFile('/local.html')
    remote_url = self.ReplaceHostName(self.GetHttpUrlForFile('/remote.html'),
                                      'localhost')
    self._http_server.SetDataForPath('/local.html',
      bytes('<p>DONE!</p>', 'utf-8'))
    self._http_server.SetDataForPath('/remote.html',
      bytes('<a href="%s">To Local</a>' % local_url, 'utf-8'))
    self._http_server.SetDataForPath('/main.html',
      bytes('<iframe src="%s">' % remote_url, 'utf-8'))
    # The first iteration after session creation frequently succeeds.
    # Therefore we make two iterations.
    for _ in range(0, 2):
      self._driver.SwitchToMainFrame()
      self._driver.Load(self.GetHttpUrlForFile('/main.html'))
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'iframe')) > 0,
        timeout=1))
      frame = self._driver.FindElement('tag name', 'iframe')
      self._driver.SwitchToFrame(frame)
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'a')) > 0,
        timeout=1))
      anchor = self._driver.FindElement('tag name', 'a')
      anchor.Click()
      self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.FindElements('tag name', 'p')) > 0,
        timeout=1))
      time.sleep(0.3)
      paragraph = self._driver.FindElement('tag name', 'p')
      self.assertEqual('DONE!', paragraph.GetText())

  def testTransientWindows(self):
    """Test that appearance and disappearance of tabs / windows does not lead
    to failure of commands that internally update the list of the top level
    WebView's.
    """
    url_for_child_window = self.GetHttpUrlForFile('/child.html')
    self._http_server.SetDataForPath('/child.html',
      bytes('''
            <script>
              setTimeout(() => {
                close();
              }, 100);
            </script>
            <p>Child Window</p>
            ''', 'utf-8'))
    self._http_server.SetDataForPath(
        '/main.html',
        bytes('''
          <script>
            const tab_count = 10;
            for (let k = 0; k < tab_count; ++k) {
              setTimeout(() => {
                window.open('%s', 'opened-tabs', 'noopener');
             }, 300 + 50*k);
            }
          </script>
          ''' % (url_for_child_window),
         'utf-8'))

    self._driver.Load(self.GetHttpUrlForFile('/main.html'))
    self.WaitForCondition(
        lambda: len(self._driver.GetWindowHandles()) > 1,
        timeout=1)
    self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.GetWindowHandles()) <= 1,
        timeout=10))


class ChromeDriverPageLoadTimeoutTest(ChromeDriverBaseTestWithWebServer):

  class _RequestHandler(object):
    def __init__(self):
      self.request_received_event = threading.Event()
      self.send_response_event = threading.Event()

    def handle(self, request):
      self.request_received_event.set()
      # Don't hang infinitely, 10 seconds are enough.
      self.send_response_event.wait(10)
      self.send_response_event.clear()
      return {'Cache-Control': 'no-store'}, b'Hi!'

  def setUp(self):
    self._handler = ChromeDriverPageLoadTimeoutTest._RequestHandler()
    self._http_server.SetCallbackForPath('/hang', self._handler.handle)
    super(ChromeDriverPageLoadTimeoutTest, self).setUp()

    self._driver = self.CreateDriver(
        chrome_switches=['host-resolver-rules=MAP * 127.0.0.1'])
    self._initial_url = self.GetHttpUrlForFile('/chromedriver/empty.html')
    self._driver.Load(self._initial_url)
    # When send_response_event is set, navigating to the hang URL takes only
    # about 0.1 second on Linux and Windows, but takes half a second or longer
    # on Mac. So we use longer timeout on Mac, 0.5 second on others.
    timeout = 3000 if util.GetPlatformName() == 'mac' else 500
    self._driver.SetTimeouts({'pageLoad': timeout})

  def tearDown(self):
    super(ChromeDriverPageLoadTimeoutTest, self).tearDown()
    self._http_server.SetCallbackForPath('/hang', None)

  def _LoadHangingUrl(self, host=None):
    self._driver.Load(self._http_server.GetUrl(host) + '/hang')

  def _CheckPageLoadTimeout(self, action):
    self._handler.request_received_event.clear()
    timed_out = False
    try:
      action()
    except chromedriver.Timeout as e:
      timed_out = True
    finally:
      self._handler.send_response_event.set()

    self.assertTrue(timed_out)
    # Verify that the browser actually made that request.
    self.assertTrue(self._handler.request_received_event.wait(1))

  def testPageLoadTimeout(self):
    self._CheckPageLoadTimeout(self._LoadHangingUrl)
    self.assertEqual(self._initial_url, self._driver.GetCurrentUrl())

  def testPageLoadTimeoutCrossDomain(self):
    # Cross-domain navigation is likely to be a cross-process one. In this case
    # DevToolsAgentHost behaves quite differently and does not send command
    # responses if the navigation hangs, so this case deserves a dedicated test.
    self._CheckPageLoadTimeout(lambda: self._LoadHangingUrl('foo.bar'))
    self.assertEqual(self._initial_url, self._driver.GetCurrentUrl())

  def testHistoryNavigationWithPageLoadTimeout(self):
    # Allow the page to load for the first time.
    self._handler.send_response_event.set()
    self._LoadHangingUrl()
    self.assertTrue(self._handler.request_received_event.wait(1))

    self._driver.GoBack()
    self._CheckPageLoadTimeout(self._driver.GoForward)
    self.assertEqual(self._initial_url, self._driver.GetCurrentUrl())

  def testHistoryNavigationAndLoadWithPageLoadTimeout(self):
    # Allow the page to load for the first time.
    self._handler.send_response_event.set()
    self._LoadHangingUrl()
    self.assertTrue(self._handler.request_received_event.wait(1))

    self._driver.GoBack()
    self._CheckPageLoadTimeout(self._LoadHangingUrl)
    self.assertEqual(self._initial_url, self._driver.GetCurrentUrl())

  def testRefreshWithPageLoadTimeout(self):
    # Allow the page to load for the first time.
    self._handler.send_response_event.set()
    self._LoadHangingUrl()
    self.assertTrue(self._handler.request_received_event.wait(1))

    self._CheckPageLoadTimeout(self._driver.Refresh)


class ChromeDriverAndroidTest(ChromeDriverBaseTest):
  """End to end tests for Android-specific tests."""

  def testLatestAndroidAppInstalled(self):
    if ('stable' not in _ANDROID_PACKAGE_KEY and
        'beta' not in _ANDROID_PACKAGE_KEY):
      return

    self._driver = self.CreateDriver()

    try:
      omaha_list = json.loads(
          urllib.request.urlopen('http://omahaproxy.appspot.com/all.json').read())
      for l in omaha_list:
        if l['os'] != 'android':
          continue
        for v in l['versions']:
          if (('stable' in v['channel'] and 'stable' in _ANDROID_PACKAGE_KEY) or
              ('beta' in v['channel'] and 'beta' in _ANDROID_PACKAGE_KEY)):
            omaha = list(map(int, v['version'].split('.')))
            device = list(map(int,
              self._driver.capabilities['browserVersion'].split('.')))
            self.assertTrue(omaha <= device)
            return
      raise RuntimeError('Malformed omaha JSON')
    except urllib.error.URLError as e:
      print('Unable to fetch current version info from omahaproxy (%s)' % e)

  def testDeviceManagement(self):
    self._drivers = [self.CreateDriver()
                     for _ in device_utils.DeviceUtils.HealthyDevices()]
    self.assertRaises(chromedriver.UnknownError, self.CreateDriver)
    self._drivers[0].Quit()
    self._drivers[0] = self.CreateDriver()

  def testAndroidGetWindowSize(self):
    self._driver = self.CreateDriver()
    size = self._driver.GetWindowRect()

    script_size = self._driver.ExecuteScript(
        'return [window.outerWidth, window.outerHeight, 0, 0]')
    self.assertEqual(size, script_size)

    script_inner = self._driver.ExecuteScript(
        'return [window.innerWidth * visualViewport.scale, '
        'window.innerHeight * visualViewport.scale]')
    # Subtract inner size by 1 to compensate for rounding errors.
    self.assertLessEqual(script_inner[0] - 1, size[0])
    self.assertLessEqual(script_inner[1] - 1, size[1])
    # Sanity check: screen dimensions in the range 20-20000px
    self.assertLessEqual(size[0], 20000)
    self.assertLessEqual(size[1], 20000)
    self.assertGreaterEqual(size[0], 20)
    self.assertGreaterEqual(size[1], 20)

class ChromeDownloadDirTest(ChromeDriverBaseTest):

  def RespondWithCsvFile(self, request):
    return {'Content-Type': 'text/csv'}, b'a,b,c\n1,2,3\n'

  def WaitForFileToDownload(self, path):
    deadline = monotonic() + 60
    while True:
      time.sleep(0.1)
      if os.path.isfile(path) or monotonic() > deadline:
        break
    self.assertTrue(os.path.isfile(path), "Failed to download file!")

  def testFileDownloadWithClick(self):
    download_dir = self.CreateTempDir()
    download_name = os.path.join(download_dir, 'a_red_dot.png')
    driver = self.CreateDriver(download_dir=download_dir)
    driver.Load(ChromeDriverTest.GetHttpUrlForFile(
        '/chromedriver/download.html'))
    driver.FindElement('css selector', '#red-dot').Click()
    self.WaitForFileToDownload(download_name)
    self.assertEqual(
        ChromeDriverTest.GetHttpUrlForFile('/chromedriver/download.html'),
        driver.GetCurrentUrl())

  def testFileDownloadAfterTab(self):
      download_dir = self.CreateTempDir()
      download_name = os.path.join(download_dir, 'a_red_dot.png')
      driver = self.CreateDriver(download_dir=download_dir)
      driver.Load(ChromeDriverTest.GetHttpUrlForFile(
          '/chromedriver/empty.html'))
      new_window = driver.NewWindow(window_type='tab')
      driver.SwitchToWindow(new_window['handle'])
      driver.Load(ChromeDriverTest.GetHttpUrlForFile(
          '/chromedriver/download.html'))
      driver.FindElement('css selector', '#red-dot').Click()
      self.WaitForFileToDownload(download_name)
      self.assertEqual(
          ChromeDriverTest.GetHttpUrlForFile('/chromedriver/download.html'),
          driver.GetCurrentUrl())

  def testFileDownloadWithGet(self):
    ChromeDriverTest._http_server.SetCallbackForPath(
        '/abc.csv', self.RespondWithCsvFile)
    download_dir = self.CreateTempDir()
    driver = self.CreateDriver(download_dir=download_dir)
    original_url = driver.GetCurrentUrl()
    driver.Load(ChromeDriverTest.GetHttpUrlForFile('/abc.csv'))
    self.WaitForFileToDownload(os.path.join(download_dir, 'abc.csv'))
    self.assertEqual(original_url, driver.GetCurrentUrl())

  def testDownloadDirectoryOverridesExistingPreferences(self):
    user_data_dir = self.CreateTempDir()
    download_dir = self.CreateTempDir()
    sub_dir = os.path.join(user_data_dir, 'Default')
    os.mkdir(sub_dir)
    prefs_file_path = os.path.join(sub_dir, 'Preferences')

    prefs = {
      'test': 'this should not be changed',
      'download': {
        'default_directory': '/old/download/directory'
      }
    }

    with open(prefs_file_path, 'w') as f:
      json.dump(prefs, f)

    self.CreateDriver(
        chrome_switches=['user-data-dir=' + user_data_dir],
        download_dir=download_dir)

    with open(prefs_file_path) as f:
      prefs = json.load(f)

    self.assertEqual('this should not be changed', prefs['test'])
    download = prefs['download']
    self.assertEqual(download['default_directory'], download_dir)


class ChromeSwitchesCapabilityTest(ChromeDriverBaseTest):
  """Tests that chromedriver properly processes chromeOptions.args capabilities.

  Makes sure the switches are passed to Chrome.
  """

  def assertDebuggerAddressIsConfigured(self, driver):
    self.assertIsNotNone(driver.debuggerAddress)
    self.assertIsInstance(driver.debuggerAddress, str)
    self.assertNotEqual('', driver.debuggerAddress)
    # The check detects the regression of https://crbug.com/chromedriver/4533
    self.assertNotEqual(':-1', driver.debuggerAddress)
    components = driver.debuggerAddress.split(':')
    conn = http.client.HTTPConnection(components[0], components[1])
    conn.request('GET', '/json/version')
    resp = conn.getresponse()
    resp = json.loads(resp.read())
    self.assertIn('Browser', resp)
    self.assertIn('webSocketDebuggerUrl', resp)

  def testSwitchWithoutArgument(self):
    """Tests that switch --dom-automation can be passed to Chrome.

    Unless --dom-automation is specified, window.domAutomationController
    is undefined.
    """
    driver = self.CreateDriver(chrome_switches=['dom-automation'])
    self.assertNotEqual(
        None,
        driver.ExecuteScript('return window.domAutomationController'))

  def testRemoteDebuggingPort(self):
    """Tests that passing --remote-debugging-port through capabilities works.
    """
    # Must use retries since there is an inherent race condition in port
    # selection.
    ports_generator = util.FindProbableFreePorts()
    exception = None
    for _ in range(3):
      port = next(ports_generator)
      port_flag = 'remote-debugging-port=%s' % port
      try:
        driver = self.CreateDriver(chrome_switches=[port_flag])
      except Exception as e:
        exception = e
        continue
      driver.Load('chrome:version')
      command_line = driver.FindElement('css selector',
                                        '#command_line').GetText()
      driver.Quit()
      self.assertIn(port_flag, command_line)
      break
    if exception is not None:
      raise exception

  def testRemoteDebuggingPipe(self):
    """Tests that passing --remote-debugging-pipe through capabilities works.
    """
    pipe_flag = 'remote-debugging-pipe'
    driver = self.CreateDriver(chrome_switches=[pipe_flag])
    driver.Load('chrome:version')
    command_line = driver.FindElement('css selector',
                                      '#command_line').GetText()
    self.assertIn(pipe_flag, command_line)

  def testUnspportedRemoteDebuggingPipe(self):
    """Tests that passing --remote-debugging-pipe through capabilities works.
    """
    # CBOR mode is not yet supported
    with self.assertRaisesRegex(chromedriver.ChromeDriverException,
                                "only ASCIIZ protocol mode is supported"):
      self.CreateDriver(chrome_switches=["remote-debugging-pipe=cbor"])
    # Invalid mode value
    with self.assertRaisesRegex(chromedriver.ChromeDriverException,
                                "only ASCIIZ protocol mode is supported"):
      self.CreateDriver(chrome_switches=["remote-debugging-pipe=xyz"])

  def testDebuggerAddressByDefault(self):
    """Tests that capabilities returned by the "New Session" command contain
    a workable debuggerAddress url.
    """
    driver = self.CreateDriver()
    self.assertDebuggerAddressIsConfigured(driver)

  def testDebuggerAddressForRemoteDebuggingPort(self):
    """Tests that capabilities returned by the "New Session" command contain
    a workable debuggerAddress url. This modification explicitly enables
    communication over web sockets.
    """
    driver = self.CreateDriver(chrome_switches=['remote-debugging-port=0'])
    self.assertDebuggerAddressIsConfigured(driver)

  def testDebuggerAddressForRemoteDebuggingPipe(self):
    """Tests that capabilities returned by the "New Session" command do not
    contain a debuggerAddress url. This modification explicitly enables
    communication over pipes.
    The requirement of no debuggerAddress in the capabilities can be changed in
    the future if we decide to mimic the http endpoint of the browser on the
    ChromeDriver side.
    """
    driver = self.CreateDriver(chrome_switches=['remote-debugging-pipe'])
    self.assertIsNone(driver.debuggerAddress)

  def testDebuggerAddressForRemoteDebuggingPortAndPipe(self):
    """Tests that capabilities returned by the "New Session" command contain
    a valid debuggerAddress url. This modification explicitly enables
    communication over web sockets and pipes.
    """
    driver = self.CreateDriver(chrome_switches=[
        'remote-debugging-pipe',
        'remote-debugging-port=0'])
    self.assertDebuggerAddressIsConfigured(driver)


class ChromeDesiredCapabilityTest(ChromeDriverBaseTest):
  """Tests that chromedriver properly processes desired capabilities."""

  def testDefaultTimeouts(self):
    driver = self.CreateDriver()
    timeouts = driver.GetTimeouts()
    # Compare against defaults in W3C spec
    self.assertEqual(timeouts['implicit'], 0)
    self.assertEqual(timeouts['pageLoad'], 300000)
    self.assertEqual(timeouts['script'], 30000)

  def testTimeouts(self):
    driver = self.CreateDriver(timeouts = {
        'implicit': 123,
        'pageLoad': 456,
        'script':   789
    })
    timeouts = driver.GetTimeouts()
    self.assertEqual(timeouts['implicit'], 123)
    self.assertEqual(timeouts['pageLoad'], 456)
    self.assertEqual(timeouts['script'], 789)

  # Run in Legacy mode
  def testUnexpectedAlertBehaviourLegacy(self):
    driver = self.CreateDriver(unexpected_alert_behaviour="accept",
                               send_w3c_capability=False,
                               send_w3c_request=False)
    self.assertEqual("accept",
                      driver.capabilities['unexpectedAlertBehaviour'])
    driver.ExecuteScript('alert("HI");')
    self.WaitForCondition(driver.IsAlertOpen)
    self.assertRaisesRegex(chromedriver.UnexpectedAlertOpen,
                            'unexpected alert open: {Alert text : HI}',
                            driver.FindElement, 'tag name', 'div')
    self.assertFalse(driver.IsAlertOpen())

  def testUnexpectedAlertBehaviourW3c(self):
    driver = self.CreateDriver(unexpected_alert_behaviour='accept',
                               send_w3c_capability=True, send_w3c_request=True)
    self.assertEqual('accept',
                      driver.capabilities['unhandledPromptBehavior'])
    driver.ExecuteScript('alert("HI");')
    self.WaitForCondition(driver.IsAlertOpen)
    # With unhandledPromptBehavior=accept, calling GetTitle (and most other
    # endpoints) automatically dismisses the alert, so IsAlertOpen() becomes
    # False afterwards.
    self.assertEqual(driver.GetTitle(), '')
    self.assertFalse(driver.IsAlertOpen())


class ChromeExtensionsCapabilityTest(ChromeDriverBaseTestWithWebServer):
  """Tests that chromedriver properly processes chromeOptions.extensions."""

  def _PackExtension(self, ext_path):
    return base64.b64encode(open(ext_path, 'rb').read()).decode('utf-8')

  def testExtensionsInstall(self):
    """Checks that chromedriver can take the extensions in crx format."""
    crx_1 = os.path.join(_TEST_DATA_DIR, 'ext_test_1.crx')
    crx_2 = os.path.join(_TEST_DATA_DIR, 'ext_test_2.crx')
    self.CreateDriver(chrome_extensions=[self._PackExtension(crx_1),
                                         self._PackExtension(crx_2)])

  def testExtensionsInstallZip(self):
    """Checks that chromedriver can take the extensions in zip format."""
    zip_1 = os.path.join(_TEST_DATA_DIR, 'ext_test_1.zip')
    self.CreateDriver(chrome_extensions=[self._PackExtension(zip_1)])

  def testCanInspectBackgroundPage(self):
    crx = os.path.join(_TEST_DATA_DIR, 'ext_bg_page.crx')
    # This test exercises inspection of an extension background page, which
    # is only valid for manifest V2 extensions. Explicitly disable the
    # experiment that disallows MV2 extensions.
    # This test can be removed entirely when support for MV2 extensions is
    # removed.
    driver = self.CreateDriver(
        chrome_extensions=[self._PackExtension(crx)],
        experimental_options={'windowTypes': ['background_page']},
        chrome_switches=['disable-features=ExtensionManifestV2Disabled'])
    handles = driver.GetWindowHandles()
    for handle in handles:
      driver.SwitchToWindow(handle)
      if driver.GetCurrentUrl() == 'chrome-extension://' \
          'nibbphkelpaohebejnbojjalikodckih/_generated_background_page.html':
        self.assertEqual(42, driver.ExecuteScript('return magic;'))
        return
    self.fail("couldn't find generated background page for test extension")

  def testIFrameWithExtensionsSource(self):
    crx_path = os.path.join(_TEST_DATA_DIR, 'frames_extension.crx')
    driver = self.CreateDriver(
        chrome_extensions=[self._PackExtension(crx_path)])
    driver.Load(
        ChromeDriverTest._http_server.GetUrl() +
          '/chromedriver/iframe_extension.html')
    driver.SwitchToFrame('testframe')
    element = driver.FindElement('css selector', '#p1')
    self.assertEqual('Its a frame with extension source', element.GetText())

  def testDontExecuteScriptsInContentScriptContext(self):
    # This test extension has a content script which runs in all frames (see
    # https://developer.chrome.com/extensions/content_scripts) which causes each
    # frame on the page to be associated with multiple JS execution contexts.
    # Make sure that ExecuteScript operates on the page's context, rather than
    # the extension's content script's one.
    extension_path = os.path.join(_TEST_DATA_DIR, 'all_frames')
    driver = self.CreateDriver(
        chrome_switches=['load-extension=%s' % extension_path])
    driver.Load(
        ChromeDriverTest._http_server.GetUrl() + '/chromedriver/container.html')
    driver.SwitchToMainFrame()
    self.assertEqual('one', driver.ExecuteScript("return window['global_var']"))
    driver.SwitchToFrame('iframe')
    self.assertEqual('two', driver.ExecuteScript("return window['iframe_var']"))

class ChromeLogPathCapabilityTest(ChromeDriverBaseTest):
  """Tests that chromedriver properly processes chromeOptions.logPath."""

  LOG_MESSAGE = 'Welcome to ChromeLogPathCapabilityTest!'

  def testChromeLogPath(self):
    """Checks that user can specify the path of the chrome log.

    Verifies that a log message is written into the specified log file.
    """
    tmp_log_path = tempfile.NamedTemporaryFile()
    driver = self.CreateDriver(chrome_log_path=tmp_log_path.name)
    driver.ExecuteScript('console.info("%s")' % self.LOG_MESSAGE)
    driver.Quit()
    self.assertTrue(self.LOG_MESSAGE in open(tmp_log_path.name).read())


class MobileEmulationCapabilityTest(ChromeDriverBaseTestWithWebServer):
  """Tests that ChromeDriver processes chromeOptions.mobileEmulation.

  Makes sure the device metrics are overridden in DevTools and user agent is
  overridden in Chrome.
  """

  def getHighEntropyClientHints(self, driver):
    return driver.ExecuteScript('''
                         return navigator.userAgentData.getHighEntropyValues([
                          'architecture',
                          'bitness',
                          'model',
                          'platformVersion',
                          'fullVersionList',
                          'wow64'])''')

  # Run in Legacy mode
  def testDeviceMetricsWithStandardWidth(self):
    driver = self.CreateDriver(
        send_w3c_capability=False, send_w3c_request=False,
        mobile_emulation = {
            'deviceMetrics': {'width': 360, 'height': 640, 'pixelRatio': 3},
            'userAgent': 'Mozilla/5.0 (Linux; Android 4.2.1; en-us; Nexus 5 Bui'
                         'ld/JOP40D) AppleWebKit/535.19 (KHTML, like Gecko) Chr'
                         'ome/18.0.1025.166 Mobile Safari/535.19'
            })
    driver.SetWindowRect(600, 400, None, None)
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertTrue(driver.capabilities['mobileEmulationEnabled'])
    self.assertEqual(360, driver.ExecuteScript('return window.screen.width'))
    self.assertEqual(640, driver.ExecuteScript('return window.screen.height'))

  # Run in Legacy mode
  def testDeviceMetricsWithDeviceWidth(self):
    driver = self.CreateDriver(
        send_w3c_capability=False, send_w3c_request=False,
        mobile_emulation = {
            'deviceMetrics': {'width': 360, 'height': 640, 'pixelRatio': 3},
            'userAgent': 'Mozilla/5.0 (Linux; Android 4.2.1; en-us; Nexus 5 Bui'
                         'ld/JOP40D) AppleWebKit/535.19 (KHTML, like Gecko) Chr'
                         'ome/18.0.1025.166 Mobile Safari/535.19'
            })
    driver.Load(self._http_server.GetUrl() + '/userAgentUseDeviceWidth')
    self.assertTrue(driver.capabilities['mobileEmulationEnabled'])
    self.assertEqual(360, driver.ExecuteScript('return window.screen.width'))
    self.assertEqual(640, driver.ExecuteScript('return window.screen.height'))

  def testUserAgent(self):
    driver = self.CreateDriver(
        mobile_emulation = {'userAgent': 'Agent Smith'})
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    body_tag = driver.FindElement('tag name', 'body')
    self.assertEqual('Agent Smith', body_tag.GetText())

  def testDeviceName(self):
    driver = self.CreateDriver(
        mobile_emulation = {'deviceName': 'Nexus 5'})
    driver.Load(self._http_server.GetUrl() + '/userAgentUseDeviceWidth')
    self.assertEqual(360, driver.ExecuteScript('return window.screen.width'))
    self.assertEqual(640, driver.ExecuteScript('return window.screen.height'))
    body_tag = driver.FindElement('tag name', 'body')
    self.assertRegex(
        body_tag.GetText(),
        '^' +
        re.escape('Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) '
                  'AppleWebKit/537.36 (KHTML, like Gecko) Chrome/') +
        r'\d+\.\d+\.\d+\.\d+' +
        re.escape(' Mobile Safari/537.36') + '$')

  def testSendKeysToElement(self):
    driver = self.CreateDriver(
        mobile_emulation = {'deviceName': 'Nexus 5'})
    text = driver.ExecuteScript(
        'document.body.innerHTML = \'<input type="text">\';'
        'var input = document.getElementsByTagName("input")[0];'
        'input.addEventListener("change", function() {'
        '  document.body.appendChild(document.createElement("br"));'
        '});'
        'return input;')
    text.SendKeys('0123456789+-*/ Hi')
    text.SendKeys(', there!')
    value = driver.ExecuteScript('return arguments[0].value;', text)
    self.assertEqual('0123456789+-*/ Hi, there!', value)

  def testClickElement(self):
    driver = self.CreateDriver(
        mobile_emulation = {'deviceName': 'Nexus 5'})
    driver.Load('about:blank')
    div = driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("click", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    div.Click()
    self.assertEqual(1, len(driver.FindElements('tag name', 'br')))

  # Run in Legacy mode
  def testTapElement(self):
    driver = self.CreateDriver(
        send_w3c_capability=False, send_w3c_request=False,
        mobile_emulation = {'deviceName': 'Nexus 5'})
    driver.Load('about:blank')
    div = driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'var div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("touchstart", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')
    div.SingleTap()
    self.assertEqual(1, len(driver.FindElements('tag name', 'br')))

  def testNetworkConnectionDisabledByDefault(self):
    driver = self.CreateDriver()
    self.assertFalse(driver.capabilities['networkConnectionEnabled'])

  def testNetworkConnectionUnsupported(self):
    driver = self.CreateDriver()
    # Network connection capability must be enabled to set/retrieve
    self.assertRaises(chromedriver.UnknownError,
                      driver.GetNetworkConnection)

    self.assertRaises(chromedriver.UnknownError,
                      driver.SetNetworkConnection, 0x1)

  # Run in Legacy mode
  def testNetworkConnectionEnabled(self):
    # mobileEmulation must be enabled for networkConnection to be enabled
    driver = self.CreateDriver(
        mobile_emulation={'deviceName': 'Nexus 5'},
        network_connection=True,
        send_w3c_capability=False, send_w3c_request=False)
    self.assertTrue(driver.capabilities['mobileEmulationEnabled'])
    self.assertTrue(driver.capabilities['networkConnectionEnabled'])

  def testEmulateNetworkConnection4g(self):
    driver = self.CreateDriver(
        mobile_emulation={'deviceName': 'Nexus 5'},
        network_connection=True)
    # Test 4G connection.
    connection_type = 0x8
    returned_type = driver.SetNetworkConnection(connection_type)
    self.assertEqual(connection_type, returned_type)
    network = driver.GetNetworkConnection()
    self.assertEqual(network, connection_type)

  def testEmulateNetworkConnectionMultipleBits(self):
    driver = self.CreateDriver(
        mobile_emulation={'deviceName': 'Nexus 5'},
        network_connection=True)
    # Connection with 4G, 3G, and 2G bits on.
    # Tests that 4G takes precedence.
    connection_type = 0x38
    returned_type = driver.SetNetworkConnection(connection_type)
    self.assertEqual(connection_type, returned_type)
    network = driver.GetNetworkConnection()
    self.assertEqual(network, connection_type)

  def testWifiAndAirplaneModeEmulation(self):
    driver = self.CreateDriver(
        mobile_emulation={'deviceName': 'Nexus 5'},
        network_connection=True)
    # Connection with both Wifi and Airplane Mode on.
    # Tests that Wifi takes precedence over Airplane Mode.
    connection_type = 0x3
    returned_type = driver.SetNetworkConnection(connection_type)
    self.assertEqual(connection_type, returned_type)
    network = driver.GetNetworkConnection()
    self.assertEqual(network, connection_type)

  def testNetworkConnectionTypeIsAppliedToAllTabsImmediately(self):
    def respondWithString(request):
      return {}, bytes("""
        <html>
        <body>%s</body>
        </html>""" % "hello world!", 'utf-8')

    self._http_server.SetCallbackForPath(
      '/helloworld', respondWithString)

    driver = self.CreateDriver(
        mobile_emulation={'deviceName': 'Nexus 5'},
        network_connection=True)

    # Set network to online
    connection_type = 0x10
    returned_type = driver.SetNetworkConnection(connection_type)
    self.assertEqual(connection_type, returned_type)

    # Open a window with two divs counting successful + unsuccessful
    # attempts to complete XML task
    driver.Load(
        self._http_server.GetUrl() +'/chromedriver/xmlrequest_test.html')
    window1_handle = driver.GetCurrentWindowHandle()
    old_handles = driver.GetWindowHandles()
    driver.FindElement('css selector', '#requestButton').Click()

    driver.FindElement('css selector', '#link').Click()
    new_window_handle = self.WaitForNewWindow(driver, old_handles)
    self.assertNotEqual(None, new_window_handle)
    driver.SwitchToWindow(new_window_handle)
    self.assertEqual(new_window_handle, driver.GetCurrentWindowHandle())

    # Set network to offline to determine whether the XML task continues to
    # run in the background, indicating that the conditions are only applied
    # to the current WebView
    connection_type = 0x1
    returned_type = driver.SetNetworkConnection(connection_type)
    self.assertEqual(connection_type, returned_type)

    driver.SwitchToWindow(window1_handle)
    connection_type = 0x1

  def testNetworkConnectionTypeIsAppliedToAllTabs(self):
    driver = self.CreateDriver(
        mobile_emulation={'deviceName': 'Nexus 5'},
        network_connection=True)
    driver.Load(self._http_server.GetUrl() +'/chromedriver/page_test.html')
    window1_handle = driver.GetCurrentWindowHandle()
    old_handles = driver.GetWindowHandles()

    # Test connection is offline.
    connection_type = 0x1
    returned_type = driver.SetNetworkConnection(connection_type)
    self.assertEqual(connection_type, returned_type)
    network = driver.GetNetworkConnection()
    self.assertEqual(network, connection_type)

    # Navigate to another window.
    driver.FindElement('css selector', '#link').Click()
    new_window_handle = self.WaitForNewWindow(driver, old_handles)
    self.assertNotEqual(None, new_window_handle)
    driver.SwitchToWindow(new_window_handle)
    self.assertEqual(new_window_handle, driver.GetCurrentWindowHandle())
    self.assertRaises(
        chromedriver.NoSuchElement, driver.FindElement, 'css selector', '#link')

    # Set connection to 3G in second window.
    connection_type = 0x10
    returned_type = driver.SetNetworkConnection(connection_type)
    self.assertEqual(connection_type, returned_type)

    driver.SwitchToWindow(window1_handle)
    self.assertEqual(window1_handle, driver.GetCurrentWindowHandle())

    # Test whether first window has old or new network conditions.
    network = driver.GetNetworkConnection()
    self.assertEqual(network, connection_type)

  def testDefaultComplianceMode(self):
    driver = self.CreateDriver(send_w3c_capability=None,
                               send_w3c_request=True)
    self.assertTrue(driver.w3c_compliant)

  def testW3cCompliantResponses(self):
    # It's an error to send Legacy format request
    # without Legacy capability flag.
    with self.assertRaises(chromedriver.InvalidArgument):
      self.CreateDriver(send_w3c_request=False)

    # It's an error to send Legacy format capability
    # without Legacy request flag.
    with self.assertRaises(chromedriver.SessionNotCreated):
      self.CreateDriver(send_w3c_capability=False)

    # Can enable W3C capability in a W3C format request.
    driver = self.CreateDriver(send_w3c_capability=True)
    self.assertTrue(driver.w3c_compliant)

    # Can enable W3C request in a legacy format request.
    driver = self.CreateDriver(send_w3c_request=True)
    self.assertTrue(driver.w3c_compliant)

    # Asserts that errors are being raised correctly in the test client
    # with a W3C compliant driver.
    self.assertRaises(chromedriver.UnknownError,
                      driver.GetNetworkConnection)

    # Can set Legacy capability flag in a Legacy format request.
    driver = self.CreateDriver(send_w3c_capability=False,
                               send_w3c_request=False)
    self.assertFalse(driver.w3c_compliant)

  def testClientHintsMobileLegacy(self):
    expected_ua = ''.join(['Mozilla/5.0 (Linux; Android 4.2.1; en-us; Nexus 5',
                          'Build/JOP40D) AppleWebKit/535.19',
                          '(KHTML, like Gecko) Chrome/18.0.1025.166 Mobile',
                           'Safari/535.19'])
    driver = self.CreateDriver(
        mobile_emulation = {
            'deviceMetrics': {'width': 360, 'height': 640, 'pixelRatio': 3},
            'userAgent': expected_ua
            })
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual(expected_ua, driver.ExecuteScript(
        'return navigator.userAgent'))
    self.assertEqual('Android', driver.ExecuteScript(
        'return navigator.userAgentData.platform'))
    self.assertEqual(True, driver.ExecuteScript(
        'return navigator.userAgentData.mobile'))
    hints = self.getHighEntropyClientHints(driver)
    self.assertEqual('', hints['architecture'])
    self.assertEqual('', hints['bitness'])
    self.assertEqual('', hints['model'])
    self.assertEqual('', hints['platformVersion'])
    self.assertEqual(False, hints['wow64'])

  def testClientHintsNonMobileLegacy(self):
    expected_ua = 'Mozilla/5.0 (Linux; Android 4.2.1; en-us; Nexus 5 Bui'
    'ld/JOP40D) AppleWebKit/535.19 (KHTML, like Gecko) Chr'
    'ome/18.0.1025.166 Safari/535.19'
    driver = self.CreateDriver(
        mobile_emulation = {
            'userAgent': expected_ua
            })
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual(expected_ua, driver.ExecuteScript(
        'return navigator.userAgent'))
    self.assertEqual("Android", driver.ExecuteScript(
        'return navigator.userAgentData.platform'))
    self.assertEqual(False, driver.ExecuteScript(
        'return navigator.userAgentData.mobile'))
    hints = self.getHighEntropyClientHints(driver)
    self.assertEqual('', hints['architecture'])
    self.assertEqual('', hints['bitness'])
    self.assertEqual('', hints['model'])
    self.assertEqual('', hints['platformVersion'])
    self.assertEqual(False, hints['wow64'])

  def testClientHintsInferredMobileUA(self):
    driver = self.CreateDriver(
        mobile_emulation = {
            'deviceMetrics': {'width': 360, 'height': 640, 'pixelRatio': 3},
            'clientHints': {
                'platform': 'Android',
                'platformVersion': '17',
                'architecture': 'arm',
                'bitness': '32',
                'mobile': True,
                'model': 'Special',
            }
        })
    major_version = driver.capabilities['browserVersion'].split('.')[0]
    expected_ua = ''.join(('Mozilla/5.0 (Linux; Android 10; K) ',
                          'AppleWebKit/537.36 (KHTML, like Gecko) ',
                          f'Chrome/{major_version}.0.0.0 Mobile ',
                          'Safari/537.36'))

    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual(expected_ua, driver.ExecuteScript(
        'return navigator.userAgent'))
    self.assertEqual('Android', driver.ExecuteScript(
        'return navigator.userAgentData.platform'))
    self.assertEqual(True, driver.ExecuteScript(
        'return navigator.userAgentData.mobile'))
    hints = self.getHighEntropyClientHints(driver)
    self.assertEqual('arm', hints['architecture'])
    self.assertEqual('32', hints['bitness'])
    self.assertEqual('Special', hints['model'])
    self.assertEqual('17', hints['platformVersion'])
    self.assertEqual(False, hints['wow64'])

  def testClientHintsInferredDesktopUA(self):
    driver = self.CreateDriver(
        mobile_emulation = {
            'clientHints': {
                'platform': 'Windows',
                'platformVersion': '12',
                'architecture': 'x86',
                'bitness': '64',
            }
        })
    major_version = driver.capabilities['browserVersion'].split('.')[0]
    expected_ua = ''.join(('Mozilla/5.0 (Windows NT 10.0; Win64; x64) ',
                          'AppleWebKit/537.36 (KHTML, like Gecko) ',
                           f'Chrome/{major_version}.0.0.0 Safari/537.36'))

    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual(expected_ua, driver.ExecuteScript(
        'return navigator.userAgent'))
    self.assertEqual('Windows', driver.ExecuteScript(
        'return navigator.userAgentData.platform'))
    self.assertEqual(False, driver.ExecuteScript(
        'return navigator.userAgentData.mobile'))
    hints = self.getHighEntropyClientHints(driver)
    self.assertEqual('x86', hints['architecture'])
    self.assertEqual('64', hints['bitness'])
    self.assertEqual('', hints['model'])
    self.assertEqual('12', hints['platformVersion'])
    self.assertEqual(False, hints['wow64'])

  def testClientHintsDeviceNameNexus5(self):
    driver = self.CreateDriver(
        mobile_emulation = {'deviceName': 'Nexus 5'})
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual('Android', driver.ExecuteScript(
        'return navigator.userAgentData.platform'))
    self.assertEqual(True, driver.ExecuteScript(
        'return navigator.userAgentData.mobile'))
    hints = self.getHighEntropyClientHints(driver)
    self.assertEqual('', hints['architecture'])
    self.assertEqual('', hints['bitness'])
    self.assertEqual('Nexus 5', hints['model'])
    self.assertEqual('6.0', hints['platformVersion'])
    self.assertEqual(False, hints['wow64'])
    major_version = driver.capabilities['browserVersion'].split('.')[0]
    expected_ua = ''.join(('Mozilla/5.0 ',
                           '(Linux; Android 6.0; Nexus 5 Build/MRA58N) ',
                           'AppleWebKit/537.36 (KHTML, like Gecko) ',
                           f'Chrome/{major_version}.0.0.0 ',
                           'Mobile Safari/537.36'))
    actual_ua = driver.ExecuteScript('return navigator.userAgent')
    self.assertEqual(expected_ua, actual_ua)

  def testClientHintsDeviceNameIPhoneX(self):
    driver = self.CreateDriver(
        mobile_emulation = {'deviceName': 'iPhone X'})
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual('', driver.ExecuteScript(
        'return navigator.userAgentData.platform'))
    self.assertEqual(True, driver.ExecuteScript(
        'return navigator.userAgentData.mobile'))
    hints = self.getHighEntropyClientHints(driver)
    self.assertEqual('', hints['architecture'])
    self.assertEqual('', hints['bitness'])
    self.assertEqual('', hints['model'])
    self.assertEqual('', hints['platformVersion'])
    self.assertEqual(False, hints['wow64'])
    expected_ua = ''.join(('Mozilla/5.0 ',
                           '(iPhone; CPU iPhone OS 13_2_3 like Mac OS X) ',
                           'AppleWebKit/605.1.15 (KHTML, like Gecko) ',
                           'Version/13.0.3 ',
                           'Mobile/15E148 Safari/604.1'))
    actual_ua = driver.ExecuteScript('return navigator.userAgent')
    self.assertEqual(expected_ua, actual_ua)

  def testClientHintsDeviceNameIPad(self):
    driver = self.CreateDriver(
        mobile_emulation = {'deviceName': 'iPad'})
    driver.Load(self._http_server.GetUrl() + '/userAgent')
    self.assertEqual('', driver.ExecuteScript(
        'return navigator.userAgentData.platform'))
    self.assertEqual(False, driver.ExecuteScript(
        'return navigator.userAgentData.mobile'))
    hints = self.getHighEntropyClientHints(driver)
    self.assertEqual('', hints['architecture'])
    self.assertEqual('', hints['bitness'])
    self.assertEqual('', hints['model'])
    self.assertEqual('', hints['platformVersion'])
    self.assertEqual(False, hints['wow64'])
    expected_ua = ''.join(('Mozilla/5.0 (iPad; CPU OS 11_0 like Mac OS X) ',
                           'AppleWebKit/604.1.34 (KHTML, like Gecko) ',
                           'Version/11.0 Mobile/15A5341f Safari/604.1'
                           ))
    actual_ua = driver.ExecuteScript('return navigator.userAgent')
    self.assertEqual(expected_ua, actual_ua)


class ChromeDriverLogTest(ChromeDriverBaseTest):
  """Tests that chromedriver produces the expected log file."""

  UNEXPECTED_CHROMEOPTION_CAP = 'unexpected_chromeoption_capability'
  LOG_MESSAGE = 'unrecognized chrome option: %s' % UNEXPECTED_CHROMEOPTION_CAP

  def testChromeDriverLog(self):
    _, tmp_log_path = tempfile.mkstemp(prefix='chromedriver_log_')
    chromedriver_server = server.Server(
        _CHROMEDRIVER_BINARY, log_path=tmp_log_path)
    try:
      driver = chromedriver.ChromeDriver(
          chromedriver_server.GetUrl(), chromedriver_server.GetPid(),
          chrome_binary=_CHROME_BINARY,
          http_timeout=_HTTP_TIMEOUT,
          experimental_options={ self.UNEXPECTED_CHROMEOPTION_CAP : 1 })
      driver.Quit()
    except chromedriver.ChromeDriverException as e:
      self.assertTrue(self.LOG_MESSAGE in str(e))
    finally:
      chromedriver_server.Kill()
    with open(tmp_log_path, 'r') as f:
      self.assertTrue(self.LOG_MESSAGE in f.read())

  def testDisablingDriverLogsSuppressesChromeDriverLog(self):
    _, tmp_log_path = tempfile.mkstemp(prefix='chromedriver_log_')
    chromedriver_server = server.Server(
        _CHROMEDRIVER_BINARY, log_path=tmp_log_path, verbose=False)
    try:
      driver = self.CreateDriver(
          chromedriver_server.GetUrl(), logging_prefs={'driver':'OFF'})
      driver.Load(
        ChromeDriverTest._http_server.GetUrl() + '/chromedriver/empty.html')
      driver.AddCookie({'name': 'secret_code', 'value': 'bosco'})
      driver.Quit()
    finally:
      chromedriver_server.Kill()
    with open(tmp_log_path, 'r') as f:
      self.assertNotIn('bosco', f.read())


class ChromeLoggingCapabilityTest(ChromeDriverBaseTest):
  """Tests chromedriver tracing support and Inspector event collection."""

  def testPerformanceLogger(self):
    driver = self.CreateDriver(
        experimental_options={'perfLoggingPrefs': {
            'traceCategories': 'blink.console'
          }}, logging_prefs={'performance':'ALL'})
    driver.Load(
        ChromeDriverTest._http_server.GetUrl() + '/chromedriver/empty.html')
    # Mark the timeline; later we will verify the marks appear in the trace.
    driver.ExecuteScript('console.time("foobar")')
    driver.ExecuteScript('console.timeEnd("foobar")')
    logs = driver.GetLog('performance')
    driver.Quit()

    marked_timeline_events = []
    seen_log_domains = {}
    for entry in logs:
      devtools_message = json.loads(entry['message'])['message']
      method = devtools_message['method']
      domain = method[:method.find('.')]
      seen_log_domains[domain] = True
      if method != 'Tracing.dataCollected':
        continue
      self.assertTrue('params' in devtools_message)
      self.assertTrue(isinstance(devtools_message['params'], dict))
      cat = devtools_message['params'].get('cat', '')
      if (cat == 'blink.console' and
          devtools_message['params']['name'] == 'foobar'):
        marked_timeline_events.append(devtools_message)
    self.assertEqual(2, len(marked_timeline_events))
    self.assertEqual({'Network', 'Page', 'Tracing'},
                      set(seen_log_domains.keys()))

  def testDevToolsEventsLogger(self):
    """Tests that the correct event type (and no other) is logged"""
    event = 'Page.loadEventFired'
    driver = self.CreateDriver(
        devtools_events_to_log=[event], logging_prefs={'devtools':'ALL'})
    driver.Load('about:blank')
    logs = driver.GetLog('devtools')
    for entry in logs:
      devtools_message = json.loads(entry['message'])
      method = devtools_message['method']
      self.assertTrue('params' in devtools_message)
      self.assertEqual(event, method)

class SessionHandlingTest(ChromeDriverBaseTest):
  """Tests for session operations."""
  def testQuitASessionMoreThanOnce(self):
    driver = self.CreateDriver()
    driver.Quit()
    driver.Quit()

  def testGetSessions(self):
    driver = self.CreateDriver()
    response = driver.GetSessions()
    self.assertEqual(1, len(response))

    driver2 = self.CreateDriver()
    response = driver2.GetSessions()
    self.assertEqual(2, len(response))


class RemoteBrowserTest(ChromeDriverBaseTest):
  """Tests for ChromeDriver remote browser capability."""
  def setUp(self):
    self.assertTrue(_CHROME_BINARY is not None,
                    'must supply a chrome binary arg')

  def testConnectToRemoteBrowser(self):
    # Must use retries since there is an inherent race condition in port
    # selection.
    ports_generator = util.FindProbableFreePorts()
    exception = None
    for _ in range(3):
      exception = None
      port = next(ports_generator)
      temp_dir = util.MakeTempDir()
      print('temp dir is ' + temp_dir)
      cmd = [_CHROME_BINARY,
             '--remote-debugging-port=%d' % port,
             '--user-data-dir=%s' % temp_dir,
             '--use-mock-keychain',
             '--password-store=basic',
             'data:,']
      process = subprocess.Popen(cmd)
      try:
        driver = self.CreateDriver(debugger_address='localhost:%d' % port)
        driver.ExecuteScript('console.info("%s")' % 'connecting at %d!' % port)
        driver.Quit()
      except Exception as e:
        exception = e

      # The process must be closed on each iteration otherwise a resource leak
      # happens.
      if process.poll() is None:
        process.terminate()
        # Wait for Chrome to exit here to prevent a race with Chrome to
        # delete/modify the temporary user-data-dir.
        # Maximum wait ~1 second.
        for _ in range(20):
          if process.poll() is not None:
            break
          print('continuing to wait for Chrome to exit')
          time.sleep(.05)
        else:
          process.kill()
      break
    if exception is not None:
      raise exception


class LaunchDesktopTest(ChromeDriverBaseTest):
  """Tests that launching desktop Chrome works."""

  def testExistingDevToolsPortFile(self):
    """If a DevTools port file already exists before startup, then we should
    ignore it and get our debug port number from the new file."""
    user_data_dir = tempfile.mkdtemp()
    try:
      dev_tools_port_file = os.path.join(user_data_dir, 'DevToolsActivePort')
      with open(dev_tools_port_file, 'w') as fd:
        fd.write('34\n/devtools/browser/2dab5fb1-5571-40d8-a6ad-98823bc5ff84')
      self.CreateDriver(
          chrome_switches=['user-data-dir=' + user_data_dir,
                           '--remote-debugging-port=0'])
      with open(dev_tools_port_file, 'r') as fd:
        port = int(fd.readlines()[0])
      # Ephemeral ports are always high numbers.
      self.assertTrue(port > 100)
    finally:
      shutil.rmtree(user_data_dir, ignore_errors=True)

  def testHelpfulErrorMessage_NormalExit(self):
    """If Chrome fails to start, we should provide a useful error message."""
    if util.IsWindows():
      # Not bothering implementing a Windows test since then I would have
      # to implement Windows-specific code for a program that quits and ignores
      # any arguments. Linux and Mac should be good enough coverage.
      return

    exception = None
    with tempfile.NamedTemporaryFile() as f:
      f.file.write(b'#!/bin/bash\nexit 0')
      f.file.close()
      os.chmod(f.name, 0o777)
      switches = ['--remote-debugging-port=0']
      try:
        driver = chromedriver.ChromeDriver(_CHROMEDRIVER_SERVER_URL,
                                           _CHROMEDRIVER_SERVER_PID,
                                           chrome_binary=f.name,
                                           http_timeout=_HTTP_TIMEOUT,
                                           chrome_switches=switches,
                                           test_name=self.id())
        # The constructor above must throw an exception.
        # Therefore normally this code should be unreachable.
        driver.Quit()
      except Exception as e:
        exception = e
    self.assertIsInstance(exception, chromedriver.SessionNotCreated)
    # This test can flake rarely on a heavily loaded system.
    # Scenario: the browser process starts, it looses its slice
    # and does not resume until the ChromeDriver command times out.
    # Setting "browserStartupTimeout" to some huge value will not help because
    # the http client in the CommandExecutor has a relatively small timeout for
    # HTTP requests.
    # S/A: //chrome/teest/chromedriver/client/command_executor.py
    self.assertIn('Chrome failed to start', str(exception))
    self.assertIn('exited normally', str(exception))
    self.assertIn('ChromeDriver is assuming that Chrome has crashed',
                  str(exception))

  def testHelpfulErrorMessage_NormalExitIfTimedOut(self):
    """If Chrome times out to start, we should provide a useful error message.
    This is a regression test for https://crbug.com/1467496
    """
    if util.IsWindows():
      # Not implemented on Windows due to the same reasons as explained in
      # testHelpfulErrorMessage_NormalExit
      return

    with tempfile.NamedTemporaryFile() as f:
      f.file.write(b'#!/bin/bash\nsleep 1\nexit 0')
      f.file.close()
      os.chmod(f.name, 0o777)
      switches = ['--remote-debugging-port=0']
      experimental_options = {'browserStartupTimeout': 0}
      driver = None
      with self.assertRaises(chromedriver.SessionNotCreated):
        driver = chromedriver.ChromeDriver(_CHROMEDRIVER_SERVER_URL,
                                           _CHROMEDRIVER_SERVER_PID,
                                           chrome_binary = f.name,
                                           http_timeout=_HTTP_TIMEOUT,
                                           chrome_switches = switches,
                                           experimental_options =
                                              experimental_options,
                                           test_name=self.id())
        # The constructor above must throw an exception.
        # Therefore normally this code should be unreachable.
        driver.Quit()

  def testNoBinaryErrorMessage(self):
    temp_dir = tempfile.mkdtemp()
    exception_raised = False
    try:
      driver = chromedriver.ChromeDriver(
          _CHROMEDRIVER_SERVER_URL,
          _CHROMEDRIVER_SERVER_PID,
          chrome_binary=os.path.join(temp_dir, 'this_file_should_not_exist'),
          http_timeout=_HTTP_TIMEOUT,
          test_name=self.id())
      # The constructor above must throw an exception.
      # Therefore normally this code should be unreachable.
      driver.Quit()
    except Exception as e:
      self.assertIn('no chrome binary', str(e))
      exception_raised = True
    finally:
      shutil.rmtree(temp_dir)
    self.assertTrue(exception_raised)


class PerfTest(ChromeDriverBaseTest):
  """Tests for ChromeDriver perf."""

  def _RunDriverPerfTest(self, name, test_func):
    """Runs a perf test ChromeDriver server.

    Args:
      name: The name of the perf test.
      test_func: Called with the server url to perform the test action. Must
                 return the time elapsed.
    """
    result = []

    for iteration in range(10):
      result += [test_func(_CHROMEDRIVER_SERVER_URL)]

    def PrintResult(result):
      mean = sum(result) / len(result)
      avg_dev = sum([abs(sample - mean) for sample in result]) / len(result)
      print('perf result', name, mean, avg_dev, result)
      util.AddBuildStepText('%s: %.3f+-%.3f' % (
          name, mean, avg_dev))

    # Discard first result, which may be off due to cold start.
    PrintResult(result[1:])

  def testSessionStartTime(self):
    def Run(url):
      start = monotonic()
      driver = self.CreateDriver(url)
      end = monotonic()
      driver.Quit()
      return end - start
    self._RunDriverPerfTest('session start', Run)

  def testSessionStopTime(self):
    def Run(url):
      driver = self.CreateDriver(url)
      start = monotonic()
      driver.Quit()
      end = monotonic()
      return end - start
    self._RunDriverPerfTest('session stop', Run)

  def testColdExecuteScript(self):
    def Run(url):
      driver = self.CreateDriver(url)
      start = monotonic()
      driver.ExecuteScript('return 1')
      end = monotonic()
      driver.Quit()
      return end - start
    self._RunDriverPerfTest('cold exe js', Run)


class InvalidCertificateTest(ChromeDriverBaseTestWithWebServer):
  """End to end tests for ChromeDriver."""

  @staticmethod
  def GetHttpsUrlForFile(file_path):
    return (
      InvalidCertificateTest._https_server.GetUrl() + file_path)

  def setUp(self):
    self._driver = self.CreateDriver(accept_insecure_certs=True)

  def testLoadsPage(self):
    print("loading")
    self._driver.Load(self.GetHttpsUrlForFile('/chromedriver/page_test.html'))
    # Verify that page content loaded.
    self._driver.FindElement('css selector', '#link')

  def testNavigateNewWindow(self):
    print("loading")
    self._driver.Load(self.GetHttpsUrlForFile('/chromedriver/page_test.html'))
    self._driver.ExecuteScript(
        'document.getElementById("link").href = "page_test.html";')

    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('css selector', '#link').Click()
    new_window_handle = self.WaitForNewWindow(self._driver, old_handles)
    self.assertNotEqual(None, new_window_handle)
    self._driver.SwitchToWindow(new_window_handle)
    self.assertEqual(new_window_handle, self._driver.GetCurrentWindowHandle())
    # Verify that page content loaded in new window.
    self._driver.FindElement('css selector', '#link')


class PureBidiTest(ChromeDriverBaseTestWithWebServer):

  def setUp(self):
    super().setUp()
    self._connections = []

  def tearDown(self):
    for conn in self._connections:
      try:
        conn.PostCommand({
          'method': 'session.end',
          'params': {}
        })
        conn.Close()
      except chromedriver.WebSocketConnectionClosedException:
        pass
    super().tearDown()

  def createSessionNewCommand(self, browser_name=None, chrome_binary=None):
    if browser_name is None:
      browser_name = _BROWSER_NAME
    if chrome_binary is None:
      chrome_binary = _CHROME_BINARY
    options = {
      'w3c': True
    }
    android_package = None
    if _ANDROID_PACKAGE_KEY:
      android_package = constants.PACKAGE_INFO[_ANDROID_PACKAGE_KEY].package
    # The code concerning chrome switches was carried over from
    # client/chromedriver.py
    chrome_switches = []
    if sys.platform.startswith('linux') and android_package is None:
      # Workaround for crbug.com/611886.
      chrome_switches.append('no-sandbox')
      # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1695
      chrome_switches.append('disable-gpu')
    chrome_switches.append('force-color-profile=srgb')
    # Resampling can change the distance of a synthetic scroll.
    chrome_switches.append('disable-features=ResamplingScrollEvents')
    options['args'] = chrome_switches
    # TODO(crbug.com/40101714): Work around a bug with headless on Mac.
    if (util.GetPlatformName() == 'mac' and
        browser_name == 'chrome-headless-shell'):
      options['excludeSwitches'] = ['--enable-logging']
    if chrome_binary is not None:
      options['binary'] = chrome_binary
    if _MINIDUMP_PATH:
      options['minidumpPath'] =  _MINIDUMP_PATH
    return {
        'method': 'session.new',
        'params': {
            'capabilities': {
                'alwaysMatch': {
                    'browserName': browser_name,
                    'goog:chromeOptions': options,
                    'goog:testName': self.id(),
                }
            }
        }
    }


  def createWebSocketConnection(self, session_id = None):
    server_url = _CHROMEDRIVER_SERVER_URL
    conn = WebSocketConnection(server_url, session_id = session_id)
    conn.SetTimeout(5 * 60) # 5 minutes
    self._connections.append(conn)
    return conn

  def testSessionStatus(self):
    conn = self.createWebSocketConnection()
    status = conn.SendCommand({
      'method': 'session.status',
      'params': {}
    })
    self.assertEqual(True, status['ready'])
    self.assertEqual('ChromeDriver ready for new sessions.',
                     status['message'])

  def testSessionNew(self):
    conn = self.createWebSocketConnection()
    response = conn.SendCommand(self.createSessionNewCommand())
    self.assertRegex(response['sessionId'], '\\w+')

  def testSessionNewWithUnknownBrowserName(self):
    """Tests that unknown browserName in the capabilities is treated as error
    """
    conn = self.createWebSocketConnection()
    with self.assertRaises(chromedriver.SessionNotCreated):
      conn.SendCommand({
        'method': 'session.new',
        'params': {
            'capabilities': {
                'alwaysMatch': {
                    'browserName': 'quick-brown-dog',
                }
            }
        }
      })

  def testStatusInActiveSession(self):
    conn = self.createWebSocketConnection()
    conn.SendCommand(self.createSessionNewCommand())
    status = conn.SendCommand({
      'method': 'session.status',
      'params': {}
    })
    self.assertEqual(False, status['ready'])
    self.assertEqual('already connected', status['message'])

  def testSessionNewIfSessionAlreadyCreated(self):
    conn = self.createWebSocketConnection()
    conn.SendCommand(self.createSessionNewCommand())
    with self.assertRaisesRegex(chromedriver.SessionNotCreated,
                  "session not created: session already exists"):
      conn.SendCommand(self.createSessionNewCommand())

  def testAnySessionCommand(self):
    conn = self.createWebSocketConnection()
    conn.SendCommand(self.createSessionNewCommand())
    response = conn.SendCommand({
      'method': 'browsingContext.getTree',
      'params': {
      }
    })
    context = response['contexts'][0]['context']
    self.assertIsNotNone(context)

  def testUnknownStaticCommand(self):
    conn = self.createWebSocketConnection()
    with self.assertRaisesRegex(chromedriver.UnknownCommand,
                                'unknown command: abracadabra'):
      conn.SendCommand({
        'method': 'abracadabra',
        'params': {}
      })

  def testStaticCommandWithoutMethod(self):
    conn = self.createWebSocketConnection()
    with self.assertRaisesRegex(chromedriver.InvalidArgument,
                                'no\\s+\'method\''):
      conn.SendCommand({
        'params': {}
      })

  def testStaticCommandWithoutParams(self):
    conn = self.createWebSocketConnection()
    with self.assertRaisesRegex(chromedriver.InvalidArgument,
                                'no\\s+\'params\''):
      conn.SendCommand({
          'method': 'session.status'
      })

  def testSessionCommandInEndedSession(self):
    conn = self.createWebSocketConnection()
    conn.SendCommand(self.createSessionNewCommand())
    conn.SendCommand({
      'method': 'session.end',
      'params': {
      }
    })
    with self.assertRaises(chromedriver.WebSocketConnectionClosedException):
      conn.SendCommand({
        'method': 'browsingContext.getTree',
        'params': {
        }
      })

  def testSessionCommandInEndedSessionNoWait(self):
    conn = self.createWebSocketConnection()
    conn.SendCommand(self.createSessionNewCommand())
    conn.PostCommand({
      'method': 'session.end',
      'params': {
      }
    })
    # Depending on timing the command might fail with either
    # WebSocketConnectionClosedException or it might return an error code
    # "invalid session id".
    try:
      conn.SendCommand({
        'method': 'browsingContext.getTree',
        'params': {
        }
      })
    except chromedriver.WebSocketConnectionClosedException:
      pass
    except chromedriver.InvalidSessionId:
      # This situation can happen if "browsingContext.getTree" arrives before
      # the ExecuteBidiSessionEnd response is returned to the CMD thread.
      pass

  def testBrowsingContextTreeIsNotEmpty(self):
    # For the moment being the probability of failure on Mac with 20 iterations
    # is greater than 50%.
    for _ in range(0, 20):
      with self.createWebSocketConnection() as conn:
        create_session_cmd_id = conn.PostCommand(self.createSessionNewCommand())

        cmd_id_list = []
        create_session_response = None
        initial_timeout = conn.GetTimeout()
        conn.SetTimeout(0.1)

        while create_session_response is None:
          try:
            # Sending the browsingContext.getTree without waiting for the
            # response in the attempt to catch the BiDiMapper by surprise before
            # it was notified about the new tab creation.
            cmd_id = conn.PostCommand({
              'method': 'browsingContext.getTree',
              'params': {
              }
            })
            cmd_id_list.append(cmd_id)
            create_session_response = conn.WaitForResponse(
                create_session_cmd_id)
          except chromedriver.WebSocketTimeoutException:
            pass

        # Extra request after the session is certainly created.
        cmd_id = conn.PostCommand({
          'method': 'browsingContext.getTree',
          'params': {
          }
        })
        cmd_id_list.append(cmd_id)

        conn.SetTimeout(initial_timeout)

        response_list = []
        for cmd_id in cmd_id_list:
          response = None
          try:
            response = conn.WaitForResponse(cmd_id)
            response_list.append(response);
          except chromedriver.InvalidSessionId:
            # Before the session is created the expected response is "invalid
            # session". After that the correct response is a non empty list.
            continue

        conn.SendCommand({
          'method': 'session.end',
          'params': {
          }
        })

        # The list must not be empty as at least the last must have hit the
        # newly created session.
        self.assertGreater(len(response_list), 0)

        for response in response_list:
          contexts = response['contexts']
          self.assertEqual(1, len(contexts))

  def testMultipleConnectionsToSameSession(self):
    conn1 = self.createWebSocketConnection()
    response = conn1.SendCommand(self.createSessionNewCommand())
    session_id = response['sessionId']
    conn2 = self.createWebSocketConnection(session_id=session_id)
    response = conn1.SendCommand({
      'method': 'browsingContext.getTree',
      'params': {
      }
    })
    context = response['contexts'][0]['context']
    conn1.PostCommand({
      'method': 'script.evaluate',
      'params': {
          'expression': 'window.test_desert = "Kalahari"',
          'target': {
              'context': context
          },
          'awaitPromise': True,
      }
    })
    response = conn2.SendCommand({
      'method': 'script.evaluate',
      'params': {
          'expression': 'window.test_desert',
          'target': {
              'context': context
          },
          'awaitPromise': True,
      }
    })
    self.assertEqual('Kalahari', response['result']['value'])

  def testParallelConnectionIsClosedOnSessionEnd(self):
    conn1 = self.createWebSocketConnection()
    response = conn1.SendCommand(self.createSessionNewCommand())
    session_id = response['sessionId']
    conn2 = self.createWebSocketConnection(session_id=session_id)
    conn2.SendCommand({
      'method': 'session.end',
      'params': {
      }
    })
    with self.assertRaises(chromedriver.WebSocketConnectionClosedException):
      conn1.SendCommand({
        'method': 'browsingContext.getTree',
        'params': {
        }
      })

  def testCloseWithUserPromptOpened(self):
    connection = self.createWebSocketConnection()
    connection.SetTimeout(60)  # 1 min as the test is likely to timeout
    connection.SendCommand(self.createSessionNewCommand())
    context1 = connection.SendCommand({
        'method': 'browsingContext.create',
        'params': {
            'type': 'tab'
        }
    })
    context_id1 = context1["context"]
    context2 = connection.SendCommand({
        'method': 'browsingContext.create',
        'params': {
            'type': 'tab'
        }
    })
    context_id2 = context2["context"]

    url = "data:text/html,<script>alert('Blocking')</script>"

    connection.SendCommand({
        'method': 'browsingContext.navigate',
        'params': {
            'url': url,
            'context': context_id1
        }
    })
    connection.SendCommand({
        'method': 'browsingContext.navigate',
        'params': {
            'url': url,
            'context': context_id2
        }
    })

    connection.SendCommand({
        'method': 'browsingContext.close',
        'params': {
            'context': context_id1
        }
    })

    # Should be able to make further calls to BiDi
    connection.SendCommand({
          'method': 'browsingContext.getTree',
          'params': {
          }
      })

  def testFocusInFirstTab(self):
    connection = self.createWebSocketConnection()
    connection.SendCommand(self.createSessionNewCommand())
    response = connection.SendCommand({
      'method': 'browsingContext.getTree',
      'params': {
      }
    })
    context = response['contexts'][0]['context']
    result = connection.SendCommand({
      'method': 'script.evaluate',
      'params': {
          'expression': 'document.hasFocus()',
          'target': {
              'context': context
          },
          'awaitPromise': True,
      }
    })
    # According to the standard the WebDriver implementation should prefer a
    # top-level browsing context (window / tab) that has system focus.
    # S/A: https://w3c.github.io/webdriver/#dfn-new-sessions
    self.assertEqual('boolean', result['result']['type'])
    self.assertTrue(result['result']['value'])


class BidiTest(ChromeDriverBaseTestWithWebServer):

  def setUp(self):
    super().setUp()
    self._driver = self.CreateDriver(web_socket_url=True)
    self._connections = []

  def tearDown(self):
    for conn in self._connections:
      conn.Close()
    super().tearDown()

  def createWebSocketConnection(self, driver=None):
    if driver is None:
      driver = self._driver
    conn = driver.CreateWebSocketConnection()
    conn.SetTimeout(5 * 60) # 5 minutes
    self._connections.append(conn)
    return conn

  def createWebSocketConnectionIPv6(self, driver=None):
    if driver is None:
      driver = self._driver
    conn = driver.CreateWebSocketConnectionIPv6()
    conn.SetTimeout(5 * 60) # 5 minutes
    self._connections.append(conn)
    return conn

  def getContextId(self, conn, idx):
    response = conn.SendCommand({
      'method': 'browsingContext.getTree',
      'params': {
      }
    })
    return response['contexts'][idx]['context']

  def postEvaluate(self, conn, expression, context_id=None, channel=None,
                   id=None):
    if context_id is None:
      context_id = self.getContextId(conn, 0)
    command = {
      'method': 'script.evaluate',
      'params': {
          'expression': expression,
          'awaitPromise': False,
          'target': {
            'context': context_id
          }
      }
    }
    if channel is not None:
      command['channel'] = channel
    if id is not None:
      command['id'] = id
    return conn.PostCommand(command)

  @staticmethod
  def GetHttpsUrlForFile(file_path, host=None):
    return ChromeDriverSecureContextTest._https_server.GetUrl(
        host) + file_path

  def testCreateContext(self):
    conn = self.createWebSocketConnection()
    old_handles = self._driver.GetWindowHandles()
    self.assertEqual(1, len(old_handles))
    self.assertNotEqual("BiDi Mapper", self._driver.GetTitle())
    conn.SendCommand({
      'method': 'browsingContext.create',
      'params': {
          'type': 'tab'
      }
    })
    new_handles = self._driver.GetWindowHandles()
    diff = set(new_handles) - set(old_handles)
    self.assertEqual(1, len(diff))

  def testGetBrowsingContextTree(self):
    conn = self.createWebSocketConnection()
    response = conn.SendCommand({
      'method': 'browsingContext.getTree',
      'params': {
      }
    })
    contexts = response['contexts']
    self.assertEqual(1, len(contexts))

  def testMapperIsNotDisplacedByNavigation(self):
    self._http_server.SetDataForPath('/page.html',
     bytes('<html><title>Regular Page</title></body></html>', 'utf-8'))
    conn = self.createWebSocketConnection()
    old_handles = self._driver.GetWindowHandles()
    self._driver.Load(self._http_server.GetUrl() + '/page.html')
    self.assertEqual("Regular Page", self._driver.GetTitle())
    conn.SendCommand({
      'method': 'browsingContext.create',
      'params': {
          'type': 'tab'
      }
    })
    new_handles = self._driver.GetWindowHandles()
    diff = set(new_handles) - set(old_handles)
    self.assertEqual(1, len(diff))

  def testMapperIsNotDisplayedByGetWindowHandles(self):
    """Regression test for crbug.com/chromedriver/4302.
    The issue manifests in frequent flakes of this test.
    """
    script = 'for (let i=0; i<10; ++i){ window.open(); }'
    self._driver.ExecuteScript(script)
    handles = self._driver.GetWindowHandles()
    self.assertEqual(11, len(handles))
    titles = []
    for handle in handles:
      self._driver.SwitchToWindow(handle)
      titles.append(self._driver.GetTitle())
    expected_titles = ['' for _ in range(0, 11)]
    self.assertListEqual(expected_titles, titles)

  def testBrowserQuitsWhenLastPageIsClosed(self):
    conn = self.createWebSocketConnection()
    handles = self._driver.GetWindowHandles()
    self.assertEqual(1, len(handles))
    self._driver.CloseWindow()
    with self.assertRaises(chromedriver.WebSocketConnectionClosedException):
      # BiDi messages cannot have negative "id".
      # Wait indefinitely until time out.
      conn.WaitForResponse(-1)

  def testCloseOneOfManyPages(self):
    conn = self.createWebSocketConnection()
    conn.SendCommand({
      'method': 'browsingContext.create',
      'params': {
          'type': 'tab'
      }
    })
    handles = self._driver.GetWindowHandles()
    self.assertEqual(2, len(handles))
    self._driver.CloseWindow()
    response = conn.SendCommand({
      'method': 'browsingContext.getTree',
      'params': {
      }
    })
    contexts = response['contexts']
    self.assertEqual(1, len(contexts))

  def testBrowserQuitsWhenLastBrowsingContextIsClosed(self):
    conn = self.createWebSocketConnection()
    context_id = self.getContextId(conn, 0)
    conn.SendCommand({
      'method': 'browsingContext.close',
      'params': {
          'context': context_id
      }
    })
    with self.assertRaises(chromedriver.WebSocketConnectionClosedException):
      # BiDi messages cannot have negative "id".
      # Wait indefinitely until time out.
      conn.WaitForResponse(-1)

  def testConnectionIsClosedIfSessionIsDestroyed(self):
    driver = self.CreateDriver(web_socket_url=True)
    conn = self.createWebSocketConnection(driver)
    driver.Quit()
    with self.assertRaises(chromedriver.WebSocketConnectionClosedException):
      # BiDi messages cannot have negative "id".
      # Wait indefinitely until time out.
      conn.WaitForResponse(-1)

  def testCmdIdCheatAndBrowserClosing(self):
    # Browser closing mechanism should not rely on command id
    conn = self.createWebSocketConnection()
    context_id = self.getContextId(conn, 0)
    cmd_id1 = None
    # overwhelm the Mapper to have enough irrelevant responses
    for _ in range(200):
      cmd_id1 = self.postEvaluate(conn,
                                  "24",
                                  context_id = context_id,
                                  id = 10005)
    cmd_id2 = conn.PostCommand({
      'id': 10005,
      'method': 'browsingContext.close',
      'params': {
          'context': context_id
      }
    })
    self.assertEqual(cmd_id1, cmd_id2)
    conn.WaitForResponse(cmd_id2)
    with self.assertRaises(chromedriver.WebSocketConnectionClosedException):
      # BiDi messages cannot have negative "id".
      # Wait indefinitely until time out.
      conn.WaitForResponse(-1)

  def testContextCountForIFrames(self):
    path = os.path.join(chrome_paths.GetTestData(), 'chromedriver',
      'nested.html')
    url = 'file://' + urllib.request.pathname2url(path)
    # This is a regression test. Loading the same url twice leads
    # to the duplication of the nested browsing context.
    self._driver.Load(url)
    self._driver.Load(url)
    conn = self.createWebSocketConnection()
    response = conn.SendCommand({
      'method': 'browsingContext.getTree',
      'params': {
      }
    })
    contexts = response['contexts']
    self.assertIsNotNone(contexts)
    self.assertIsInstance(contexts, list)
    self.assertEqual(1, len(contexts))

    parent_context = contexts[0]
    children = parent_context['children']
    self.assertIsNotNone(children)
    self.assertIsInstance(children, list)
    self.assertEqual(1, len(children))

  def testNamedChannel(self):
    conn = self.createWebSocketConnection()
    context_id = self.getContextId(conn, 0)
    self.assertIsNotNone(context_id)

    cmd_id1 = self.postEvaluate(conn, '9', channel="abc", context_id=context_id)
    cmd_id2 = self.postEvaluate(conn, '13', context_id=context_id)

    resp = conn.WaitForResponse(cmd_id2)
    self.assertEqual(13, resp['result']['value'])
    resp = conn.TryGetResponse(cmd_id1)
    self.assertIsNone(resp)
    resp = conn.WaitForResponse(cmd_id1, channel="abc")
    self.assertEqual(9, resp['result']['value'])

  def testMultipleConnections(self):
    conn1 = self.createWebSocketConnection()
    context_id = self.getContextId(conn1, 0)
    self.assertIsNotNone(context_id)
    conn2 = self.createWebSocketConnection()
    # Pre-check: make sure that the implementation does not use the same socket
    self.assertNotEqual(conn1, conn2)

    cmd_id1 = self.postEvaluate(conn1, '77', context_id = context_id)
    cmd_id2 = self.postEvaluate(conn2, '23', context_id = context_id)
    cmd_id3 = self.postEvaluate(conn1, '41', context_id = context_id)
    cmd_id4 = self.postEvaluate(conn2, '98', context_id = context_id)

    resp = conn1.WaitForResponse(cmd_id1)
    self.assertEqual(77, resp['result']['value'])
    resp = conn1.WaitForResponse(cmd_id3)
    self.assertEqual(41, resp['result']['value'])
    resp = conn2.WaitForResponse(cmd_id4)
    self.assertEqual(98, resp['result']['value'])
    resp = conn2.WaitForResponse(cmd_id2)
    self.assertEqual(23, resp['result']['value'])

  def testMultipleConnectionsDifferentIPs(self):
    conn1 = self.createWebSocketConnection()
    context_id = self.getContextId(conn1, 0)
    self.assertIsNotNone(context_id)
    conn2 = self.createWebSocketConnectionIPv6()
    # Pre-check: make sure that the implementation does not use the same socket
    self.assertNotEqual(conn1, conn2)

    cmd_id1 = self.postEvaluate(conn1, '77', context_id = context_id)
    cmd_id2 = self.postEvaluate(conn2, '23', context_id = context_id)
    cmd_id3 = self.postEvaluate(conn1, '41', context_id = context_id)
    cmd_id4 = self.postEvaluate(conn2, '98', context_id = context_id)

    resp = conn1.WaitForResponse(cmd_id1)
    self.assertEqual(77, resp['result']['value'])
    resp = conn1.WaitForResponse(cmd_id3)
    self.assertEqual(41, resp['result']['value'])
    resp = conn2.WaitForResponse(cmd_id4)
    self.assertEqual(98, resp['result']['value'])
    resp = conn2.WaitForResponse(cmd_id2)
    self.assertEqual(23, resp['result']['value'])

  def testMultipleConnectionsNamedChannels(self):
    conn1 = self.createWebSocketConnection()
    context_id = self.getContextId(conn1, 0)
    self.assertIsNotNone(context_id)
    conn2 = self.createWebSocketConnection()
    # Pre-check: make sure that the implementation does not use the same socket
    self.assertNotEqual(conn1, conn2)

    cmd_id1 = self.postEvaluate(conn1, '77', context_id=context_id, id=100,
                                channel='3')
    cmd_id2 = self.postEvaluate(conn1, '23', context_id=context_id, id=100,
                                channel='/')
    cmd_id3 = self.postEvaluate(conn2, '41', context_id=context_id, id=101,
                                channel='3')
    cmd_id4 = self.postEvaluate(conn2, '98', context_id=context_id, id=100,
                                channel='')
    cmd_id5 = self.postEvaluate(conn2, '6', context_id=context_id, id=100)

    resp = conn2.WaitForResponse(cmd_id3, channel='3')
    self.assertEqual(41, resp['result']['value'])
    resp = conn1.WaitForResponse(cmd_id1, channel = '3')
    self.assertEqual(77, resp['result']['value'])
    resp = conn1.WaitForResponse(cmd_id2, channel='/')
    self.assertEqual(23, resp['result']['value'])
    resp = conn2.WaitForResponse(cmd_id4, channel='')
    self.assertEqual(98, resp['result']['value'])
    resp = conn2.WaitForResponse(cmd_id5)
    self.assertEqual(6, resp['result']['value'])

  def subscribeToLoad(self, conn, channel=None):
    command = {
      'method': 'session.subscribe',
      'params': {
          'events': [
              'browsingContext.load']}}
    if channel is not None:
      command['channel'] = channel
    return conn.SendCommand(command, channel=channel)

  def navigateTo(self, conn, url, context_id=None, channel=None):
    if context_id is None:
      context_id = self.getContextId(conn, 0)
    command = {
        'method': 'browsingContext.navigate',
        'params': {
            'url': url,
            'wait': 'complete',
            'context': context_id}}
    if channel is not None:
      command['channel'] = channel
    return conn.SendCommand(command, channel=channel)

  def navigateSomewhere(self, conn, context_id=None, channel=None):
    return self.navigateTo(conn,
                           'data:text/html,navigated',
                           context_id=context_id,
                           channel=channel)

  def testEvent(self):
    conn = self.createWebSocketConnection()
    context_id = self.getContextId(conn, 0)
    self.assertIsNotNone(context_id)

    self.subscribeToLoad(conn)
    self.navigateSomewhere(conn, context_id)

    events = conn.TakeEvents()
    # The event for about:blank is also possible
    self.assertLessEqual(1, len(events))
    self.assertFalse('channel' in events[0])
    self.assertEqual('browsingContext.load', events[0]['method'])

  def testEventChannel(self):
    conn = self.createWebSocketConnection()
    context_id = self.getContextId(conn, 0)
    self.assertIsNotNone(context_id)

    self.subscribeToLoad(conn, channel='abc')
    self.navigateSomewhere(conn, context_id, channel='abc')

    events = conn.TakeEvents()
    # The event for about:blank is also possible
    self.assertLessEqual(1, len(events))
    self.assertEqual('abc', events[0]['channel'])
    self.assertEqual('browsingContext.load', events[0]['method'])

  def testEventChannelAndNoChannel(self):
    conn = self.createWebSocketConnection()
    context_id = self.getContextId(conn, 0)
    self.assertIsNotNone(context_id)

    self.subscribeToLoad(conn)
    self.subscribeToLoad(conn, channel='x')
    self.navigateSomewhere(conn, context_id)

    all_events = conn.TakeEvents()
    events = [evt for evt in all_events if 'channel' not in evt]
    events_x = [evt for evt in all_events
               if 'channel' in evt and evt['channel'] == 'x']

    # The event for about:blank is also possible
    self.assertLessEqual(1, len(events))
    self.assertFalse('channel' in events[0])
    self.assertEqual('browsingContext.load', events[0]['method'])

    # The event for about:blank is also possible
    self.assertLessEqual(1, len(events_x))
    self.assertEqual('x', events_x[0]['channel'])
    self.assertEqual('browsingContext.load', events[0]['method'])

  def testEventConnections(self):
    conn1 = self.createWebSocketConnection()
    conn2 = self.createWebSocketConnection()
    context_id = self.getContextId(conn1, 0)
    self.assertIsNotNone(context_id)

    self.subscribeToLoad(conn2)
    self.navigateSomewhere(conn1, context_id=context_id)
    # push the events
    cmd_id = self.postEvaluate(conn2, '12', context_id=context_id)
    conn2.WaitForResponse(cmd_id)

    events1 = conn1.TakeEvents()
    events2 = conn2.TakeEvents()

    self.assertEqual(0, len(events1))
    # The event for about:blank is also possible
    self.assertLessEqual(1, len(events2))
    self.assertFalse('channel' in events2[0])
    self.assertEqual('browsingContext.load', events2[0]['method'])

  def testElementReference(self):
    self._driver.Load(self.GetHttpUrlForFile('/chromedriver/element_ref.html'))
    element = self._driver.FindElement('css selector', '#link')
    self._driver.FindElements('tag name', 'br')
    self.assertRegex(element._id, _ELEMENT_REF_REGEX,
                     msg='Element id format is incorrect')

  def testElementReferenceNoNavigation(self):
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    self.assertRegex(div._id, _ELEMENT_REF_REGEX,
                     msg='Element id format is incorrect')

  def testCompareClassicAndBidiIds(self):
    conn = self.createWebSocketConnection()
    root_context = self.getContextId(conn, 0)
    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'return document.getElementsByTagName("div")[0];')
    classic_id = div._id
    self.assertRegex(classic_id, _ELEMENT_REF_REGEX,
                     msg='Element id format is incorrect')

    resp = conn.SendCommand({
      'method': 'script.evaluate',
      'params': {
          'expression': 'document.getElementsByTagName("div")[0]',
          'target': {
            'context': root_context
          },
          'awaitPromise': True,
          'resultOwnership': 'root',
      }
    })
    bidi_id = resp['result']['sharedId']
    self.assertRegex(bidi_id, _ELEMENT_REF_REGEX,
                     msg='Shared id format is incorrect')
    self.assertEqual(classic_id, bidi_id,
                     "Classic and BiDi id mismatch")

  def testClassicIdInBidi(self):
    conn = self.createWebSocketConnection()
    root_context = self.getContextId(conn, 0)

    div = self._driver.ExecuteScript(
        'document.body.innerHTML = "<div>old</div>";'
        'let div = document.getElementsByTagName("div")[0];'
        'div.addEventListener("click", function() {'
        '  div.innerHTML="new<br>";'
        '});'
        'return div;')

    conn.SendCommand({
      'method': 'script.callFunction',
      'params': {
          'functionDeclaration': '(elem) => elem.click()',
          'arguments': [{
            'sharedId': div._id,
          }],
          'target': {
            'context': root_context
          },
          'awaitPromise': True,
          'resultOwnership': 'root',
      }
    })
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testBidiIdInClassic(self):
    conn = self.createWebSocketConnection()
    root_context = self.getContextId(conn, 0)

    resp = conn.SendCommand({
      'method': 'script.evaluate',
      'params': {
          'expression': 'document.body.innerHTML = "<div>old</div>";'
                        'let div = document.getElementsByTagName("div")[0];'
                        'div.addEventListener("click", function() {'
                        '  div.innerHTML="new<br>";'
                        '});'
                        'div',
          'target': {
            'context': root_context
          },
          'awaitPromise': True,
          'resultOwnership': 'root',
      }
    })
    node_id = resp['result']['sharedId']
    div = webelement.WebElement(self._driver, node_id)
    div.Click()
    self.assertEqual(1, len(self._driver.FindElements('tag name', 'br')))

  def testSwitchWindows(self):
    """Regression test for crbug.com/chromedriver/4362"""
    self._http_server.SetDataForPath('/iframes.html',
       bytes('<title>iframes</title><iframe src="about:blank"/>', 'utf-8'))
    self._http_server.SetDataForPath('/original.html',
     bytes("""
       <title>original</title>
       <a href="/iframes.html" target="_blank" id="iframes">iframes</a>
       """, 'utf-8'))

    self._driver.Load(self.GetHttpUrlForFile('/original.html'))
    old_handles = self._driver.GetWindowHandles()
    self._driver.FindElement('css selector', '#iframes').Click()
    self.WaitForNewWindow(self._driver, old_handles)

    handles = self._driver.GetWindowHandles()
    titles = []
    for handle in handles:
      self._driver.SwitchToWindow(handle)
      titles.append(self._driver.GetTitle())
    self.assertEqual(len(titles), 2)
    self.assertIn('iframes', titles)
    self.assertIn('original', titles)

  def testOpenMultipleTabsInJavaScript(self):
    """Regression test for crbug.com/chromedriver/4362"""
    self._http_server.SetDataForPath('/iframes.html',
       bytes('<title>iframes</title><iframe src="about:blank"/> </body>',
             'utf-8'))
    url = self.GetHttpUrlForFile('/iframes.html')
    script = 'for (let i=0; i<10; ++i){ window.open("%s"); }' % url
    self._driver.ExecuteScript(script)
    handles = self._driver.GetWindowHandles()
    titles = []
    for handle in handles:
      self._driver.SwitchToWindow(handle)
      titles.append(self._driver.GetTitle())
    expected_titles = [''] + ['iframes' for _ in range(0, 10)]
    self.assertListEqual(expected_titles, sorted(titles))

  def testInsecureSertificatesNotAllowed(self):
    driver = self.CreateDriver(
        web_socket_url=True,
        accept_insecure_certs=False)
    conn = self.createWebSocketConnection(driver)
    page_name = self.id()
    self._https_server.SetDataForPath('/%s.html' % page_name,
       bytes('<head><<title>%s</title></head>' % page_name,
             'utf-8'))
    with self.assertRaisesRegex(chromedriver.ChromeDriverException,
                                'net::ERR_CERT_AUTHORITY_INVALID'):
      self.navigateTo(conn, self.GetHttpsUrlForFile('/%s.html' % page_name))

  def testInsecureSertificatesAllowed(self):
    driver = self.CreateDriver(
        web_socket_url=True,
        accept_insecure_certs=True)
    conn = self.createWebSocketConnection(driver)
    page_name = self.id()
    self._https_server.SetDataForPath('/%s.html' % page_name,
       bytes('<head><<title>%s</title></head>' % page_name,
             'utf-8'))
    self.navigateTo(conn, self.GetHttpsUrlForFile('/%s.html' % page_name))
    title = driver.GetTitle()
    self.assertEqual(title, page_name)

  def testFocusInFirstTab(self):
    # According to the standard the WebDriver implementation should prefer a
    # top-level browsing context (window / tab) that has system focus.
    # S/A: https://w3c.github.io/webdriver/#dfn-new-sessions
    self.assertTrue(self._driver.ExecuteScript("return document.hasFocus()"))


class CustomBidiMapperTest(ChromeDriverBaseTest):
  """Base class for testing chromedriver with a custom bidi mapper path."""

  def CreateDriver(self, bidi_mapper_path=None, **kwargs):
    log_path = os.path.join(_MINIDUMP_PATH, self.id() + '.chromedriver.log')

    chromedriver_server = server.Server(
        _CHROMEDRIVER_BINARY,
        log_path=log_path,
        bidi_mapper_path=bidi_mapper_path)

    driver = chromedriver.ChromeDriver(server_url=chromedriver_server.GetUrl(),
                                     server_pid=chromedriver_server.GetPid(),
                                     chrome_binary=_CHROME_BINARY,
                                     http_timeout=_HTTP_TIMEOUT,
                                     test_name=self.id(),
                                     web_socket_url=True,
                                     browser_name=_BROWSER_NAME,
                                     **kwargs)
    self._drivers += [driver]
    return driver

  def testInvalidCustomBidiMapperPath(self):
    # Test that an invalid bidi mapper path raises an exception.

    bidi_mapper_path = os.path.join(
        os.path.realpath(os.path.dirname(os.path.dirname(__file__))),
        'js', 'test_bidi_mapper_invalid.js')

    self.assertRaisesRegex(Exception,
                           'unknown error: ' +
                           'Failed to read the specified BiDi mapper path',
                           self.CreateDriver, bidi_mapper_path=bidi_mapper_path)

  def testValidCustomBidiMapperPath(self):
    # Test that we can use a custom bidi mapper path.

    bidi_mapper_path = os.path.join(
        os.path.realpath(os.path.dirname(os.path.dirname(__file__))),
        'js', 'test_bidi_mapper.js')

    self.assertRaisesRegex(Exception,
                           'unknown error: ' +
                           'Failed to initialize BiDi Mapper: Error: ' +
                           'custom bidi mapper error from test_bidi_mapper.js',
                           self.CreateDriver, bidi_mapper_path=bidi_mapper_path)

class ClassicTest(ChromeDriverBaseTestWithWebServer):

  def testAfterLastPage(self):
    driver = self.CreateDriver(web_socket_url=False)

    handles = driver.GetWindowHandles()
    self.assertEqual(1, len(handles))
    driver.CloseWindow()


class SupportIPv4AndIPv6(ChromeDriverBaseTest):
  def testSupportIPv4AndIPv6(self):
    has_ipv4 = False
    has_ipv6 = False
    for info in socket.getaddrinfo('localhost', 0):
      if info[0] == socket.AF_INET:
        has_ipv4 = True
      if info[0] == socket.AF_INET6:
        has_ipv6 = True
    if has_ipv4:
      self.CreateDriver("http://127.0.0.1:" +
                                 str(chromedriver_server.GetPort()))
    if has_ipv6:
      self.CreateDriver('http://[::1]:' +
                                 str(chromedriver_server.GetPort()))

class JavaScriptTests(ChromeDriverBaseTestWithWebServer):
  def GetFileUrl(self, filename):
    return 'file://' + self.js_root + filename

  def setUp(self):
    self._driver = self.CreateDriver()
    self.js_root = os.path.dirname(os.path.realpath(__file__)) + '/../js/'
    self._driver.SetWindowRect(640, 480, 0, 0)

  def checkTestResult(self):
    def getStatus():
      return self._driver.ExecuteScript('return window.CDCJStestRunStatus')

    self.WaitForCondition(getStatus)
    self.assertEqual('PASS', getStatus())

  def testElementRegionTest(self):
    self._driver.Load(self.GetFileUrl('get_element_region_test.html'))
    self.checkTestResult()

  def testAllJS(self):
    self._driver.Load(self.GetFileUrl('call_function_test.html'))
    self.checkTestResult()

    self._driver.Load(self.GetFileUrl('dispatch_touch_event_test.html'))
    self.checkTestResult()

    self._driver.Load(self.GetFileUrl('execute_async_script_test.html'))
    self.checkTestResult()

    self._driver.Load(self.GetFileUrl('execute_script_test.html'))
    self.checkTestResult()

    self._driver.Load(self.GetFileUrl('get_element_location_test.html'))
    self.checkTestResult()

    self._driver.Load(self.GetFileUrl('is_option_element_toggleable_test.html'))
    self.checkTestResult()

    self._driver.Load(self.GetFileUrl('focus_test.html'))
    self.checkTestResult()

class VendorSpecificTest(ChromeDriverBaseTestWithWebServer):

  def setUp(self):
    global _VENDOR_ID
    self._vendor_id = _VENDOR_ID
    self._driver = self.CreateDriver()

  def testGetSinks(self):
    # Regression test for chromedriver:4176
    # This command crashed ChromeDriver on Android
    self._driver.GetCastSinks(self._vendor_id)

class FedCmSpecificTest(ChromeDriverBaseTestWithWebServer):

  def setUp(self):
    port = self._https_server._server.server_port
    self._url_prefix = (self._https_server.GetUrl("localhost") +
        "/chromedriver/fedcm")
    self._default_accounts = """
{
  "id": "1234",
  "given_name": "John",
  "name": "John Doe",
  "email": "john_doe@idp.example",
  "picture": "https://idp.example/profile/123",
  "approved_clients": ["123", "456", "789"]
}, {
  "id": "5678",
  "given_name": "Aisha",
  "name": "Aisha Ahmad",
  "email": "aisha@idp.example",
  "picture": "https://idp.example/profile/567",
  "approved_clients": []
}
    """
    self._accounts = self._default_accounts
    self._token_response = bytes("""
        {
          "token": "token"
        }
        """, 'utf-8')

    def respondWithWellKnownFile(request):
      return {'Content-Type': 'application/json'}, bytes("""
      {
        "provider_urls": ["%s/fedcm.json"]
      }
      """ % self._url_prefix, 'utf-8')

    def respondWithSignedInHeader(request):
      return {'Set-Login': 'logged-in'}, b"Header sent"

    def respondWithAccountList(request):
      return {'Content-Type': 'application/json'}, bytes("""
        {"accounts": [
          %s
        ]}""" % self._accounts, 'utf-8')

    def respondWithTokenResponse(request):
      return {'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': request.GetHeader('Origin'),
        'Access-Control-Allow-Credentials': 'true'}, self._token_response

    self._https_server.SetCallbackForPath('/.well-known/web-identity',
                                          respondWithWellKnownFile)
    self._https_server.SetCallbackForPath('/chromedriver/fedcm/mark-signed-in',
                                          respondWithSignedInHeader)
    self._https_server.SetCallbackForPath('/chromedriver/fedcm/accounts.json',
                                          respondWithAccountList)
    self._https_server.SetCallbackForPath('/chromedriver/fedcm/token.json',
                                          respondWithTokenResponse)

    script_content = bytes("""
      <script>
      let promise = null;
      let abortController = new AbortController();
      function callFedCm() {
        promise = navigator.credentials.get({
          identity: {
            providers: [{
              configURL: '%s/fedcm.json',
              clientId: '123',
            }]
          },
          mediation: 'required',
          signal: abortController.signal
        });
      }
      async function getResult() {
        try {
          return (await promise).token;
        } catch (e) {
          let result = e.toString();
          if (e.name != "IdentityCredentialError") {
            return result;
          }
          if (e.code) {
            result += " Code: " + e.code;
          }
          if (e.url) {
            result += " Url: " + e.url;
          }
          return result;
        }
      }
      </script>
      """ % self._url_prefix, 'utf-8')
    self._https_server.SetDataForPath('/fedcm.html', script_content)

    self.chrome_switches = ['host-resolver-rules=MAP *:443 127.0.0.1:%s' % port,
            'enable-experimental-web-platform-features']
    self._driver = self.CreateDriver(
        accept_insecure_certs=True,
        chrome_switches=self.chrome_switches +
            ["--enable-features=FedCmIdpSigninStatusEnabled"])

    self._driver.Load(self._url_prefix + "/mark-signed-in")

  def FedCmDialogCondition(self):
    try:
        self._driver.GetDialogType()
        return True
    except:
        return False

  def FedCmNoDialogCondition(self):
    return not self.FedCmDialogCondition()

  def FedCmPopupWindowCondition(self):
    try:
        window_handles = self._driver.GetWindowHandles()
        # The two windows are the main window and the pop-up window
        if len(window_handles) == 2:
          return True
        else:
          return False
    except:
        return False

  def testGetAccounts(self):
    self._driver.Load(self._https_server.GetUrl() + '/fedcm.html')

    self._driver.SetDelayEnabled(False)
    self._driver.ResetCooldown()

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript('callFedCm()')
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))
    accounts = self._driver.GetAccounts()
    self.assertEqual(2, len(accounts))
    self.assertEqual({'title': 'Sign in to 127.0.0.1 with localhost'},
                     self._driver.GetFedCmTitle())
    self.assertEqual('AccountChooser', self._driver.GetDialogType())

    self._driver.SelectAccount(0)
    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    token = self._driver.ExecuteScript('return getResult()')
    self.assertEqual('token', token)

  def testDismissDialog(self):
    self._driver.Load(self._https_server.GetUrl() + '/fedcm.html')

    self._driver.ResetCooldown()
    self._driver.SetDelayEnabled(False)

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript('callFedCm()')
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))

    self._driver.CancelFedCmDialog()
    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    token = self._driver.ExecuteScript('return getResult()')
    self.assertEqual('NetworkError: Error retrieving a token.', token)

  def testAbortRequest(self):
    self._driver.Load(self._https_server.GetUrl() + '/fedcm.html')

    self._driver.ResetCooldown()
    self._driver.SetDelayEnabled(False)

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript('callFedCm()')
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))

    self._driver.ExecuteScript('abortController.abort()')
    self.assertTrue(self.WaitForCondition(self.FedCmNoDialogCondition))

    token = self._driver.ExecuteScript('return getResult()')
    self.assertEqual('AbortError: signal is aborted without reason', token)

  def testConfirmIdpLogin(self):
    self._accounts = ""

    self._driver.Load(self._https_server.GetUrl() + "/fedcm.html")

    self._driver.SetDelayEnabled(False)
    self._driver.ResetCooldown()

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript("callFedCm()")
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))

    accounts = self._driver.GetAccounts()
    self.assertEqual("ConfirmIdpLogin", self._driver.GetDialogType())
    self.assertEqual(0, len(accounts))

    self._accounts = self._default_accounts

    self._driver.ClickFedCmDialogButton("ConfirmIdpLoginContinue")

    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))
    accounts = self._driver.GetAccounts()
    self.assertEqual("AccountChooser", self._driver.GetDialogType())
    self.assertEqual(2, len(accounts))

    self._driver.SelectAccount(0)
    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    token = self._driver.ExecuteScript("return getResult()")
    self.assertEqual("token", token)

  def testCancelAfterFailedSelectAccount(self):
    self._accounts = ""

    self._driver.Load(self._https_server.GetUrl() + "/fedcm.html")

    self._driver.SetDelayEnabled(False)
    self._driver.ResetCooldown()

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript("callFedCm()")
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))

    accounts = self._driver.GetAccounts()
    self.assertEqual("ConfirmIdpLogin", self._driver.GetDialogType())
    self.assertEqual(0, len(accounts))

    # SelectAccount should fail, but not cause a later CancelFedCmDialog
    # to fail.
    self.assertRaises(chromedriver.InvalidArgument, self._driver.SelectAccount,
                      0)

    self._driver.CancelFedCmDialog()
    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    token = self._driver.ExecuteScript('return getResult()')
    self.assertEqual('NetworkError: Error retrieving a token.', token)

  def testClickErrorGotIt(self):
    self._token_response = bytes("""
        {
          "error": {
            "code": "invalid_request"
          }
        }
        """, 'utf-8')

    self._driver.Load(self._https_server.GetUrl() + "/fedcm.html")

    self._driver.SetDelayEnabled(False)
    self._driver.ResetCooldown()

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript("callFedCm()")
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))

    accounts = self._driver.GetAccounts()
    self.assertEqual('AccountChooser', self._driver.GetDialogType())
    self.assertEqual(2, len(accounts))

    self._driver.SelectAccount(0)

    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))
    self.assertEqual("Error", self._driver.GetDialogType())
    self._driver.ClickFedCmDialogButton("ErrorGotIt")

    error = self._driver.ExecuteScript("return getResult()")
    self.assertEqual("IdentityCredentialError: Error retrieving a token. "
                     "Code: invalid_request", error)

  def testClickErrorMoreDetails(self):
    self._token_response = bytes("""
        {
          "error": {
            "code": "invalid_request",
            "url": "/chromedriver/fedcm/more_details.html"
          }
        }
        """, 'utf-8')

    self._driver.Load(self._https_server.GetUrl() + "/fedcm.html")

    self._driver.SetDelayEnabled(False)
    self._driver.ResetCooldown()

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript("callFedCm()")
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))

    accounts = self._driver.GetAccounts()
    self.assertEqual('AccountChooser', self._driver.GetDialogType())
    self.assertEqual(2, len(accounts))

    self._driver.SelectAccount(0)

    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))
    self.assertEqual("Error", self._driver.GetDialogType())
    self._driver.ClickFedCmDialogButton("ErrorMoreDetails")

    error = self._driver.ExecuteScript("return getResult()")
    self.assertTrue(self.WaitForCondition(self.FedCmPopupWindowCondition))
    self.assertEqual("IdentityCredentialError: Error retrieving a token. "
                     "Code: invalid_request Url: " +
                     self._https_server.GetUrl("localhost") +
                     "/chromedriver/fedcm/more_details.html", error)


  def testClickPrivacyPolicy(self):
    self._driver.Load(self._https_server.GetUrl() + "/fedcm.html")

    self._driver.SetDelayEnabled(False)
    self._driver.ResetCooldown()

    self.assertRaises(chromedriver.NoSuchAlert, self._driver.GetAccounts)
    self._driver.ExecuteScript("callFedCm()")
    self.assertTrue(self.WaitForCondition(self.FedCmDialogCondition))

    accounts = self._driver.GetAccounts()
    self.assertEqual('AccountChooser', self._driver.GetDialogType())
    self.assertEqual(2, len(accounts))

    self._driver.ClickFedCmDialogButton("PrivacyPolicy", 1)
    # The dialog should not have closed.
    self.assertEqual('AccountChooser', self._driver.GetDialogType())

    # Make sure that a popup actually opened.
    self.assertTrue(self.WaitForCondition(self.FedCmPopupWindowCondition))

    self._driver.CancelFedCmDialog()


class ComputePressureSpecificTest(ChromeDriverBaseTestWithWebServer):
  """
  Tests for https://w3c.github.io/compute-pressure/#automation
  """

  def setUp(self):
    self._driver = self.CreateDriver(accept_insecure_certs=True)

    script_content = bytes(
        """
      <!DOCTYPE html>
      <script>
      let observer;
      const states = [];

      function pressureChangeCallback(records, observer) {
        for (const record of records) {
          states.push(record.state);
        }
      }

      async function addPressureObserver() {
        observer = new PressureObserver(pressureChangeCallback);
        await observer.observe("cpu");
      }
      </script>
      """, 'utf-8')
    self._https_server.SetDataForPath('/compute-pressure.html', script_content)

  def testCreateVirtualPressureSourceWithNonStringType(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "invalid argument: 'type' must be a string",
        self._driver.CreateVirtualPressureSource,
        42,
    )

  def testCreateVirtualPressureSourceWithInvalidType(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "invalid argument: Invalid pressure source: invalid_type",
        self._driver.CreateVirtualPressureSource,
        'invalid_type',
    )

  def testCreateVirtualPressureSourceWithInvalidMetadata(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "invalid argument: 'supported' must be a boolean",
        self._driver.CreateVirtualPressureSource,
        'cpu',
        {'supported': 'foo'},
    )

  def testUpdateVirtualPressureSourceWithInvalidType(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'invalid argument: Invalid pressure source: invalid_type',
        self._driver.UpdateVirtualPressureSource,
        'invalid_type',
        'nominal',
    )

  def testUpdateVirtualPressureSourceWithInvalidSample(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'invalid argument: Invalid pressure state: invalid_sample',
        self._driver.UpdateVirtualPressureSource,
        'cpu',
        'invalid_sample',
    )

  def testUpdateVirtualPressureSourceWithoutSample(self):
    self.assertRaisesRegex(
        Exception,
        "UpdateVirtualPressureSource\(\) missing 1 required " +
        "positional argument: 'sample'",
        self._driver.UpdateVirtualPressureSource,
        'cpu',
    )

  def testUpdateVirtualPressureSourceWithNonStringSample(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        "invalid argument: 'sample' must be a string",
        self._driver.UpdateVirtualPressureSource,
        "cpu",
        42,
    )

  def testUpdateVirtualPressureSourceWithoutOverriding(self):
    self.assertRaisesRegex(
        Exception,
        'invalid argument: The specified pressure source is not being '
        'overridden',
        self._driver.UpdateVirtualPressureSource, 'cpu', 'nominal')

  def testRemoveVirtualPressureSourceWithInvalidType(self):
    self.assertRaisesRegex(
        chromedriver.InvalidArgument,
        'invalid argument: Invalid pressure source: invalid_type',
        self._driver.RemoveVirtualPressureSource,
        'invalid_type',
    )

  def testRemoveVirtualPressureSourceWithoutType(self):
    self.assertRaisesRegex(
        Exception,
        "RemoveVirtualPressureSource\(\) missing 1 required " +
        "positional argument: 'type'",
        self._driver.RemoveVirtualPressureSource,
    )

  def testCreateVirtualPressureSourceNotConnected(self):
    script = """
      const done = arguments[0];
      const observer = new PressureObserver(() => {
        throw Error("The observer callback should not be called");
      });
      observer.observe("cpu").then(() => {
        throw new Error('Observation did not fail as expected');
      }, e => {
        done(e.name);
      });
    """
    self._driver.CreateVirtualPressureSource('cpu', {'supported': False})
    self._driver.Load(self._https_server.GetUrl() + '/compute-pressure.html')
    self.assertEqual('NotSupportedError',
                     self._driver.ExecuteAsyncScript(script))

  def testUpdateVirtualPressure(self):
    source = 'cpu'
    self._driver.CreateVirtualPressureSource(source)
    self._driver.Load(self._https_server.GetUrl() + '/compute-pressure.html')

    self._driver.ExecuteAsyncScript(
        'const done = arguments[0]; addPressureObserver().then(done)')
    pressure_states = ["nominal", "fair", "serious", "critical"]
    states_length = 1

    for state in pressure_states:
      self._driver.UpdateVirtualPressureSource(source, state)
      self.assertTrue(
          self.WaitForCondition(lambda: self._driver.ExecuteScript(
              'return states.length === arguments[0]', states_length)))
      received_state = self._driver.ExecuteScript('return states.at(-1)')
      self.assertEqual(state, received_state)
      states_length = states_length + 1

    self._driver.RemoveVirtualPressureSource(source)

  def testRemoveVirtualPressureSourceWhileInUse(self):
    source = 'cpu'
    self._driver.CreateVirtualPressureSource(source)
    self._driver.Load(self._https_server.GetUrl() + '/compute-pressure.html')
    self._driver.ExecuteAsyncScript(
        'const done = arguments[0]; addPressureObserver().then(done)')

    self._driver.UpdateVirtualPressureSource(source, 'serious')
    self.assertTrue(
        self.WaitForCondition(lambda: self._driver.ExecuteScript(
            'return states.at(-1) === "serious"')))
    self._driver.RemoveVirtualPressureSource(source)
    self.assertRaisesRegex(
        Exception,
        'invalid argument: The specified pressure source is not being '
        'overridden',
        self._driver.UpdateVirtualPressureSource, source, 'nominal')


class NavTrackingMitigationSpecificTest(ChromeDriverBaseTestWithWebServer):

  def setUp(self):
    global _VENDOR_ID
    self._vendor_id = _VENDOR_ID

    self._driver = self.CreateDriver(chrome_switches=[
        '--enable-features="DIPS:delete/true/'
            'triggering_action/stateful_bounce/'
            'client_bounce_detection_timeout/inf"',
        '--test-third-party-cookie-phaseout',
        '--host-resolver-rules=MAP * 127.0.0.1'
    ])

  def testRunBounceTrackingMitigations(self):
    """Test implementation of bounce tracking mitigations.
    """

    # This makes sure the underlying service exists and returns an empty
    # list before the test continues.
    self.assertTrue(self.WaitForCondition(
        lambda: len(self._driver.RunBounceTrackingMitigations()) == 0))

    initial_url = self.GetHttpUrlForFile('/initial.html')
    bounce_url = self.ReplaceHostName(
        self.GetHttpUrlForFile('/bounce.html'), 'tracker.test')
    landing_url = self.GetHttpUrlForFile('/landing.html')
    final_url = self.GetHttpUrlForFile('/final.html')

    self._http_server.SetDataForPath('/initial.html', bytes("""
        <html>
          <title>Initial Page</title>
          <body>
            <a href='%s' id='bounce'>Stateful Bounce\n</a><br>
          </body>
        </html>""" % bounce_url, 'utf-8'))

    def StatefullyBounce(request):
      return {'Set-Cookie': 'x=y'}, bytes("""
          <html>
            <title>Bounce Tracker</title>
            <body>
              <script>
                document.cookie = 'a=b'
                while (document.cookie.length == 0){}
                window.location = '%s';
              </script>
            </body>
          </html>""" % landing_url, 'utf-8')
    self._http_server.SetCallbackForPath('/bounce.html', StatefullyBounce)

    self._http_server.SetDataForPath('/landing.html', bytes("""
        <html>
          <title>Landing Page</title>
          <body>
            <a href='%s' id='final'>To Final\n</a>
          </body>
        </html>""" % final_url, 'utf-8'))

    self._http_server.SetDataForPath('/final.html', bytes("""
        <html>
          <title>DONE!</title>
        </html>""", 'utf-8'))

    self._driver.Load(initial_url)
    anchor = self._driver.FindElement('css selector', '#bounce')
    anchor.Click()

    # Waiting to be redirected to landing_url by the bounce page.
    self.assertTrue(self.WaitForCondition(
        (lambda: 'Landing Page' in self._driver.GetTitle())))

    # A click-started navigation is used to end the active redirect chain.
    anchor = self._driver.FindElement('css selector', '#final')
    anchor.Click()

    # Wait for navigation to final_url to complete.
    self.assertTrue(self.WaitForCondition(
        (lambda: 'DONE!' in self._driver.GetTitle())))

    # The DIPSService can take some time to process and record the terminated
    # redirect chain, but there is not an existing signal exposed to notify
    # when it has finished. This wait should be sufficient to allow time for it.
    self.assertTrue(self.WaitForCondition(
        lambda: "tracker.test" in
                self._driver.RunBounceTrackingMitigations(), 30, 0.5))

# 'Z' in the beginning is to make test executed in the end of suite.
class ZChromeStartRetryCountTest(unittest.TestCase):

  def testChromeStartRetryCount(self):
    self.assertEqual(0, chromedriver.ChromeDriver.retry_count,
                      "Chrome was retried to start during suite execution "
                      "in following tests:\n" +
                      ', \n'.join(chromedriver.ChromeDriver.retried_tests))

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--chromedriver',
                      help='Path to chromedriver server (REQUIRED!)')
  parser.add_argument('--log-path',
                      help='Output verbose server logs to this file')
  parser.add_argument(
      '--replayable',
      help="Don't truncate long strings in the log so that the log can be "
      "replayed.")
  parser.add_argument(
      '--failfast',
      action='store_true',
      default=False,
      help='Stop the test run on the first error or failure.')
  parser.add_argument('--chrome', help='Path to a build of the chrome binary')
  parser.add_argument(
      '--filter',
      default='',
      help='Filter for specifying what tests to run, \"*\" will run all,'
      'including tests excluded by default. E.g., *testRunMethod')
  parser.add_argument('--android-package',
                      help=('Android package key. Possible values: ' +
                            str(list(_ANDROID_NEGATIVE_FILTER.keys()))))

  parser.add_argument('--isolated-script-test-output',
                      help='JSON output file used by swarming')
  parser.add_argument(
      '--test-type',
      help='Select type of tests to run. Possible value: integration')
  parser.add_argument(
      '--vendor',
      help='Vendor id for vendor specific tests. Defaults to "goog"')
  parser.add_argument(
      '--disable-build-check',
      action='store_true',
      default=False,
      help='Allow ChromeDriver to run with an incompatible Chrome version')
  parser.add_argument(
      '--browser-name',
      default=None,
      help='Browser specific test subset selector. Inferred from the path '
           'to the binary if possible. Otherwise defaults to chrome')
  # One can run ChromeDriver under a debugger and attach the tests to the
  # running instance. Another instance of ChromeDriver server will be started
  # by the tests but they won't communicate with it. The child processes spawned
  # by the remote ChromeDriver service won't be cleaned up properly. Don't use
  # this flag in production!
  parser.add_argument(
      '--remote-chromedriver-port',
      type=int,
      default=None,
      help='Attach tests to the running instance of ChromeDriver server. '
           'This parameter is intended for debugging purposes only. Don\'t '
           'use it in production as it does not clean up resources properly')

  ##############################################################################
  # Note for other Chromium based browsers!!!
  # Path inference works only for Chrome and chrome-headless-shell.
  # Therefore you need to pass the path to your browser explicitly via the
  # --chrome argument.
  # The test set for your browser most likely coincides with the test set for
  # 'chrome'. Therefore you need to pass --browser-name=chrome or just omit this
  # argument altogether as 'chrome' is already the default value.
  ##############################################################################

  options = parser.parse_args()

  if options.chromedriver is None:
    parser.error('--chromedriver is required.\n' +
                 'Please run "%s --help" for help' % __file__)
  options.chromedriver = util.GetAbsolutePathOfUserPath(options.chromedriver)
  if (not os.path.exists(options.chromedriver) and
      util.GetPlatformName() == 'win' and
      not options.chromedriver.lower().endswith('.exe')):
    options.chromedriver = options.chromedriver + '.exe'
  if not os.path.exists(options.chromedriver):
    parser.error('Path given by --chromedriver is invalid.\n' +
                 'Please run "%s --help" for help' % __file__)

  if options.replayable and not options.log_path:
    parser.error('Need path specified when replayable log set to true.')

  # When running in commit queue & waterfall, minidump will need to write to
  # same directory as log, so use the same path
  global _MINIDUMP_PATH
  if options.log_path:
    _MINIDUMP_PATH = os.path.dirname(options.log_path)

  global _CHROMEDRIVER_BINARY
  _CHROMEDRIVER_BINARY = util.GetAbsolutePathOfUserPath(options.chromedriver)

  global _HTTP_TIMEOUT
  _HTTP_TIMEOUT = None
  if options.remote_chromedriver_port is not None:
    _HTTP_TIMEOUT = 10 * 3600 # 10 hours

  if (options.android_package and
      options.android_package not in _ANDROID_NEGATIVE_FILTER):
    parser.error('Invalid --android-package')

  additional_args = []
  if options.disable_build_check:
    additional_args.append('--disable-build-check')

  global chromedriver_server
  chromedriver_server = server.Server(
      _CHROMEDRIVER_BINARY,
      options.log_path,
      replayable=options.replayable,
      remote_chromedriver_port=options.remote_chromedriver_port,
      additional_args=additional_args)

  global _CHROMEDRIVER_SERVER_PID
  _CHROMEDRIVER_SERVER_PID = chromedriver_server.GetPid()

  global _CHROMEDRIVER_SERVER_URL
  _CHROMEDRIVER_SERVER_URL = chromedriver_server.GetUrl()

  global _CHROME_BINARY
  browser_name = options.browser_name

  if options.chrome:
    _CHROME_BINARY = util.GetAbsolutePathOfUserPath(options.chrome)
    if browser_name is None:
      browser_name = _InferBrowserName(options.chrome)
  else:
    # In some test environments (such as commit queue), it's not convenient to
    # specify Chrome binary location on the command line. Try to use heuristics
    # to locate the Chrome binary next to the ChromeDriver binary.
    driver_path = os.path.dirname(_CHROMEDRIVER_BINARY)

    if browser_name is None:
      browser_name = 'chrome'
    (_CHROME_BINARY, path_list) = _InferBrowserPath(browser_name,
                                       driver_path,
                                       util.GetPlatformName())
    if _CHROME_BINARY is None:
      joined_path_list = '  * ' + '\n  * '.join(path_list)
      if len(path_list) == 0:
        joined_path_list = '    <empty list>'

      print('''Failed to find the browser "%s".
Attempted paths:
%s
Delegating this task to ChromeDriver'''
            % (browser_name, joined_path_list))

  # If the browser lookup mechanism of the tests fails then this task is
  # delegated to ChromeDriver. The later needs to know which browser to search.
  # Therefore the browser name is stored in a global variable and passed to
  # ChromeDriver via the "browserName" capability.
  global _BROWSER_NAME
  _BROWSER_NAME = browser_name

  global _ANDROID_PACKAGE_KEY
  _ANDROID_PACKAGE_KEY = options.android_package

  if _ANDROID_PACKAGE_KEY:
    devil_chromium.Initialize()

  global _VENDOR_ID
  if options.vendor:
    _VENDOR_ID = options.vendor
  else:
    _VENDOR_ID = 'goog'

  if options.filter == '':
    if _ANDROID_PACKAGE_KEY:
      negative_filter = _ANDROID_NEGATIVE_FILTER[_ANDROID_PACKAGE_KEY]
    else:
      negative_filter = _GetDesktopNegativeFilter(_BROWSER_NAME)

    if options.test_type is not None:
      if options.test_type == 'integration':
        negative_filter += _INTEGRATION_NEGATIVE_FILTER
      else:
        parser.error('Invalid --test-type. Valid value: integration')

    options.filter = '*-' + ':__main__.'.join([''] + negative_filter)

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      sys.modules[__name__])
  test_suite = unittest_util.FilterTestSuite(all_tests_suite, options.filter)
  test_suites = [list(map(lambda t: t.id(),  test_suite))]

  ChromeDriverBaseTestWithWebServer.GlobalSetUp()

  runner = unittest.TextTestRunner(
      stream=sys.stdout, descriptions=False, verbosity=2,
      failfast=options.failfast,
      resultclass=unittest_util.AddSuccessTextTestResult)
  result = runner.run(test_suite)
  results = [result]

  num_failed = len(result.failures) + len(result.errors)
  # Limit fail tests to 10 to avoid real bug causing many tests to fail
  # Only enable retry for automated bot test
  if (num_failed > 0 and num_failed <= 10
      and options.test_type == 'integration'):
    retry_test_suite = unittest.TestSuite()
    for f in result.failures:
      retry_test_suite.addTest(f[0])
    for e in result.errors:
      retry_test_suite.addTest(e[0])
    test_suites.append(list(map(lambda t: t.id(),  retry_test_suite)))
    print('\nRetrying failed tests\n')
    retry_result = runner.run(retry_test_suite)
    results.append(retry_result)

  ChromeDriverBaseTestWithWebServer.GlobalTearDown()

  if options.isolated_script_test_output:
    util.WriteResultToJSONFile(test_suites, results,
                               options.isolated_script_test_output)
  util.TryUploadingResultToResultSink(results)
  sys.exit(len(results[-1].failures) + len(results[-1].errors))
