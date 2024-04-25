# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import random
import sys

from telemetry.internal.backends.chrome import cast_browser_finder
from telemetry.internal.browser import browser_finder
from telemetry.internal.browser import browser_finder_exceptions
from telemetry.internal.platform import cast_device
from telemetry.testing import serially_executed_browser_test_case
from telemetry.util import image_util
from telemetry.util import rgba_color


class ScreenshotTest(
    serially_executed_browser_test_case.SeriallyExecutedBrowserTestCase):
  @classmethod
  def Name(cls):
    return 'screenshot'

  @classmethod
  def SetUpProcess(cls):
    super(cls, ScreenshotTest).SetUpProcess()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()
    cls.tab = cls.browser.tabs[0]
    cls._cast_receiver_to_create.SetCastSender(cls.tab)
    cls.StartCastReceiver()
    cls.SetStaticServerDirs([os.path.join(os.path.dirname(__file__), 'data')])

  @classmethod
  def StartCastReceiver(cls):
    """ Navigates sending tab to about:blank and cast to receiver."""

    try:
      # TODO(crbug.com/40558533): Note cls._browser_options actually is a
      # FinderOptions object, and we need to access the real browser_option's
      # contained inside.
      cls._cast_receiver_to_create.SetUpEnvironment(
          cls._browser_options.browser_options)
      cls.cast_receiver = cls._cast_receiver_to_create.Create()
    except Exception:
      cls._cast_receiver_to_create.CleanUpEnvironment()
      raise

  @classmethod
  def SetBrowserOptions(cls, browser_options):
    """Sets the browser option for the browser to create.

    Args:
      browser_options: Browser options object for the browser we want to test.
    """
    cls._browser_options = browser_options
    if browser_options.local_cast:
      cls._local_cast = True
    else:
      cls._local_cast = False
    cls._browser_to_create = browser_finder.FindBrowser(browser_options)
    if not cls._browser_to_create:
      raise browser_finder_exceptions.BrowserFinderException(
          'Cannot find browser of type %s. \n\nAvailable browsers:\n%s\n' % (
              browser_options.browser_options.browser_type,
              '\n'.join(browser_finder.GetAllAvailableBrowserTypes(
                  browser_options))))

    cast_devices = cast_device.FindAllAvailableDevices(browser_options)
    cast_browsers = []
    for device in cast_devices:
      cast_browsers.extend(
          cast_browser_finder.FindAllAvailableBrowsers(
              browser_options, device))
    cls._cast_receiver_to_create = \
        cast_browser_finder.SelectDefaultBrowser(cast_browsers)

    if not cls.platform:
      cls.platform = cls._browser_to_create.platform
      cls.platform.network_controller.Open(
          browser_options.browser_options.wpr_mode)
    assert cls.platform == cls._browser_to_create.platform, (
        'All browser launches within same test suite must use browsers on '
        'the same platform')

  @classmethod
  def TearDownProcess(cls):
    """ Tear down the testing logic after running the test cases.
    This is guaranteed to be called only once for all the tests after the test
    suite finishes running.
    """

    if cls.browser:
      cls.StopBrowser()

    if cls.cast_receiver:
      cls.StopCastReceiver()

  @classmethod
  def StopCastReceiver(cls):
    assert cls.cast_receiver, 'Cast receiver is not started'
    try:
      cls.cast_receiver.Close()
      cls.cast_receiver = None
    finally:
      cls._cast_receiver_to_create.CleanUpEnvironment()

  def TestFling(self):
    """Fling a Youtube video to the Cast receiver and confirm that rendering
    is correctly performed via screenshot."""
    self.cast_receiver._browser_backend.FlingVideo(
        'https://youtu.be/zBeWNaNWtIM')
    self.receiver_tab = self.cast_receiver.tabs[0]
    screenshot = self.receiver_tab.Screenshot(10)
    logging.warning(screenshot)
    pixel_value = image_util.GetPixelColor(screenshot, 100, 100)

    if self._local_cast:
      # For Cast Core on Linux, no rendering is available yet so the screen
      # should display the Youtube icon.
      videoRGB = rgba_color.RgbaColor(40, 40, 40)
    else:
      videoRGB = rgba_color.RgbaColor(37, 150, 190)

    # Allow for off-by-one errors due to color conversion.
    tolerance = 1

    if not videoRGB.IsEqual(pixel_value, tolerance):
      error_message = ('Color mismatch : expected (%d, %d, %d), ' +
                       'got (%d, %d, %d)') % (
                           videoRGB.r, videoRGB.g, videoRGB.b,
                           pixel_value.r, pixel_value.g, pixel_value.b)
      self.fail(error_message)

  def TestMirroring(self):
    """Draw a square of random color on the sending tab, take a screenshot on
    the Cast receiver and confirm that the colors match."""

    canvasRGB = rgba_color.RgbaColor(
        random.randint(0, 255), random.randint(0, 255), random.randint(0, 255),
        255)
    tab = self.tab
    tab.action_runner.Navigate(self.UrlOfStaticFilePath('screenshot.html'))
    tab.EvaluateJavaScript('window.draw({{ red }}, {{ green }}, {{ blue }});',
                           red=canvasRGB.r,
                           green=canvasRGB.g,
                           blue=canvasRGB.b)
    self.cast_receiver._browser_backend.MirrorTab()
    self.receiver_tab = self.cast_receiver.tabs[0]

    ### TODO(crbug.com/40836534): Investigate issues grabbing a screenshot
    ### with tab mirroring.
    # screenshot = self.receiver_tab.Screenshot(10)
    # pixel_value = image_util.GetPixelColor(screenshot, 0, 0)

    # # Allow for off-by-one errors due to color conversion.
    # tolerance = 1

    # if not canvasRGB.IsEqual(pixel_value, tolerance):
    #   error_message = ('Color mismatch : expected (%d, %d, %d), ' +
    #                    'got (%d, %d, %d)') % (
    #                        canvasRGB.r, canvasRGB.g, canvasRGB.b,
    #                        pixel_value.r, pixel_value.g, pixel_value.b)
    #   self.fail(error_message)


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return serially_executed_browser_test_case \
            .LoadAllTestsInModule(sys.modules[__name__])