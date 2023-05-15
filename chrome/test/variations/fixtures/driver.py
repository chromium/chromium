# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil

import attr
import pytest

from chrome.test.variations import test_utils
from contextlib import contextmanager
from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver import ChromeOptions
from selenium.webdriver.chrome.service import Service
from typing import Optional


def pytest_addoption(parser):
  # By default, running on the hosted platform.
  parser.addoption('--target-platform',
                   default=test_utils.get_hosted_platform(),
                   dest='target_platform',
                   choices=['linux', 'win', 'mac', 'android', 'cros',
                            'lacros'],
                   help='If present, run for the target platform, '
                   'defaults to the host platform.')

  parser.addoption('--channel',
                   default='dev',
                   choices=['dev', 'canary', 'beta', 'stable', 'extended'],
                   help='The channel of Chrome to download.')

  parser.addoption('--chromedriver',
                   help='The path to the existing chromedriver. '
                   'This will ignore --channel and skip downloading.')


class DriverFactory:
  """The factory to create webdriver for the pre-defined environment"""

  @contextmanager
  def create_driver(self,
                    seed_file: Optional[str] = None,
                    options: Optional[ChromeOptions] = None
    ) -> webdriver.Remote:
    """Creates a webdriver."""
    raise NotImplemented


@attr.attrs()
class DesktopDriverFactory(DriverFactory):
  """Driver factory for desktop platforms."""
  channel: Optional[str] = attr.attrib()
  crash_dump_dir: Optional[str] = attr.attrib()
  chromedriver_path: str = attr.attrib()

  @contextmanager
  def create_driver(
    self,
    seed_file: Optional[str] = None,
    options: Optional[ChromeOptions] = None
    ) -> webdriver.Remote:
    os.environ['BREAKPAD_DUMP_LOCATION'] = self.crash_dump_dir

    options = options or ChromeOptions()
    options.add_argument('disable-field-trial-config')

    if seed_file:
      assert os.path.exists(seed_file)
      options.add_argument(f'variations-test-seed-path={seed_file}')
      options.add_argument(
        f'fake-variations-channel={self.channel}')
    options.add_experimental_option('excludeSwitches',
                                    ['disable-background-networking'])
    driver = None
    try:
      logging.info('Launching Chrome w/ caps: %s',
                   options.to_capabilities())
      driver = webdriver.Chrome(service=Service(self.chromedriver_path),
                                options=options)
      yield driver
    except WebDriverException as e:
      # Report this to be part of test result.
      if os.listdir(self.crash_dump_dir):
        logging.error('Chrome crashed and exited abnormally.\n%s', e)
      else:
        logging.error('Uncaught WebDriver exception thrown.\n%s', e)
      raise
    finally:
      if driver:
        driver.quit()
      shutil.rmtree(self.crash_dump_dir, ignore_errors=True)


# pylint: disable=redefined-outer-name
@pytest.fixture(scope="session")
def chromedriver_path(pytestconfig) -> str:
  """Returns a path to the chromedriver."""
  if cd_path := pytestconfig.getoption('chromedriver'):
    cd_path = os.path.abspath(cd_path)
    assert os.path.isfile(cd_path), (
      f'Given chromedriver doesn\'t exist. ({cd_path})')
    return cd_path

  platform = pytestconfig.getoption('target_platform')
  channel = pytestconfig.getoption('channel')

  # https://developer.chrome.com/docs/versionhistory/reference/#platform-identifiers
  downloaded_dir = None
  if platform == "linux":
    ver = test_utils.find_version('linux', channel)
    downloaded_dir = test_utils.download_chrome_linux(version=str(ver))
  elif platform == "mac":
    ver = test_utils.find_version('mac_arm64', channel)
    downloaded_dir = test_utils.download_chrome_mac(version=str(ver))
  elif platform == "win":
    ver = test_utils.find_version('win64', channel)
    downloaded_dir = test_utils.download_chrome_win(version=str(ver))
  else:
    return None

  return str(os.path.join(downloaded_dir, 'chromedriver'))


@pytest.fixture
def driver_factory(
  pytestconfig,
  chromedriver_path: str,
  tmp_path_factory: pytest.TempPathFactory
  ) -> DriverFactory:
  """Returns a factory that creates a webdriver."""
  target_platform = pytestconfig.getoption('target_platform')
  if target_platform in ('linux', 'win', 'mac'):
    return DesktopDriverFactory(
      channel=pytestconfig.getoption('channel'),
      crash_dump_dir=str(tmp_path_factory.mktemp('crash')),
      chromedriver_path=chromedriver_path)

  assert False, f'Not supported platform {target_platform}.'
