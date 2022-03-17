# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from telemetry.internal.backends.chrome import cast_browser_finder
from telemetry.internal.browser import browser_finder
from telemetry.internal.platform import cast_device
from telemetry.testing import serially_executed_browser_test_case


class InfoCollectionTestArgs():
  """Struct-like class for passing args to an InfoCollection test."""

  def __init__(self, expected_vendor_id_str=None, expected_device_id_strs=None):
    self.gpu = None
    self.expected_vendor_id_str = expected_vendor_id_str
    self.expected_device_id_strs = expected_device_id_strs


class InfoCollectionTest(
    serially_executed_browser_test_case.SeriallyExecutedBrowserTestCase):
  @classmethod
  def Name(cls):
    return 'info_collection'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(InfoCollectionTest, cls).AddCommandlineArgs(parser)
    parser.add_option(
        '--expected-device-id',
        action='append',
        dest='expected_device_ids',
        default=[],
        help='The expected device id. Can be specified multiple times.')
    parser.add_option('--expected-vendor-id', help='The expected vendor id')

  @classmethod
  def GenerateCastTests(cls, options):
    yield ('InfoCollection_basic', '_',
           ('_RunBasicTest',
            InfoCollectionTestArgs(
                expected_vendor_id_str=options.expected_vendor_id,
                expected_device_id_strs=options.expected_device_ids)))

  @classmethod
  def GenerateTestCases__RunCastTest(cls, options):
    for test_name, url, args in cls.GenerateCastTests(options):
      yield test_name, (url, test_name, args)

  @classmethod
  def SetUpProcess(cls):
    super(cls, InfoCollectionTest).SetUpProcess()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()
    cls.tab = cls.browser.tabs[0]
    cls._cast_receiver_to_create.SetCastSender(cls.tab)
    cls.StartCastReceiver()
    cls.receiver_tab = cls.cast_receiver.tabs[0]

  @classmethod
  def StartCastReceiver(cls):
    try:
      # TODO(crbug.com/803104): Note cls._browser_options actually is a
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
    cls._cast_receiver_to_create = browser_finder.FindBrowser(browser_options)
    cls._browser_to_create = browser_finder.FindBrowser(browser_options)
    cast_devices = cast_device.FindAllAvailableDevices(browser_options)
    cast_browsers = []
    for device in cast_devices:
      cast_browsers.extend(
          cast_browser_finder.FindAllAvailableBrowsers(
              browser_options, device))
    cls._cast_receiver_to_create = \
        cast_browser_finder.SelectDefaultBrowser(cast_browsers)
    if not cls._browser_to_create:
      raise browser_finder_exceptions.BrowserFinderException(
          'Cannot find browser of type %s. \n\nAvailable browsers:\n%s\n' % (
              browser_options.browser_options.browser_type,
              '\n'.join(browser_finder.GetAllAvailableBrowserTypes(
                  browser_options))))
    if not cls.platform:
      cls.platform = cls._browser_to_create.platform
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

  def _RunCastTest(self, test_path, *args):
    del test_path  # Unused in this particular test.

    # Gather the IDs detected by the GPU process
    system_info = self.cast_receiver.GetSystemInfo()
    if not system_info:
      self.fail("Browser doesn't support GetSystemInfo")
    assert len(args[1]) == 2
    test_func = args[1][0]
    test_args = args[1][1]
    assert test_args.gpu is None
    test_args.gpu = system_info.gpu
    getattr(self, test_func)(test_args)

  ######################################
  # Helper functions for the tests below

  def _RunBasicTest(self, test_args):
    device = test_args.gpu.devices[0]
    if not device:
      self.fail("System Info doesn't have a gpu")

    detected_vendor_id = device.vendor_id
    detected_device_id = device.device_id

    # Gather the expected IDs passed on the command line
    if (not test_args.expected_vendor_id_str
        or not test_args.expected_device_id_strs):
      self.fail('Missing --expected-[vendor|device]-id command line args')

    expected_vendor_id = int(test_args.expected_vendor_id_str, 16)
    expected_device_ids = [
        int(id_str, 16) for id_str in test_args.expected_device_id_strs
    ]

    # Check expected and detected GPUs match
    if detected_vendor_id != expected_vendor_id:
      self.fail('Vendor ID mismatch, expected %s but got %s.' %
                (expected_vendor_id, detected_vendor_id))

    if detected_device_id not in expected_device_ids:
      self.fail('Device ID mismatch, expected %s but got %s.' %
                (expected_device_ids, detected_device_id))


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return serially_executed_browser_test_case \
            .LoadAllTestsInModule(sys.modules[__name__])