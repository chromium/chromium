# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import os
import subprocess
import time

from contextlib import contextmanager
from pkg_resources import packaging
from typing import Optional

import attr

from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver.common import service
from selenium.webdriver.chrome.service import Service as ChromeService

DEFAULT_WAIT_TIMEOUT = 10     # 10 seconds timeout
DEFAULT_WAIT_INTERVAL = 0.3   # 0.3 seconds wait intervals

@attr.attrs()
class DriverFactory:
  """The factory to create webdriver for the pre-defined environment"""

  chromedriver_path: str = attr.attrib()
  artifacts_path: str = attr.attrib()

  def __attrs_post_init__(self):
    # The counter that counts each new webdriver session. This counter gets
    # updated each time #get_driver_service is called, and this should
    # only gets called in #create_driver
    self._driver_session_counter = 0

  @functools.cached_property
  def driver_version(self) -> packaging.version.Version:
    # Sample version output: ChromeDriver 117.0.5938.0 (branch_name)
    version_str = str(subprocess.check_output([self.chromedriver_path, '-v']))
    return packaging.version.parse(version_str.split(' ')[1])

  @property
  def driver_session_counter(self) -> int:
    return self._driver_session_counter

  @property
  def supports_startup_timeout(self) -> bool:
    """Whether the driver supports browserStartupTimeout option."""
    return self.driver_version >= packaging.version.parse('117.0.5913.0')

  @property
  def default_options(self) -> webdriver.ChromeOptions:
    """The default ChromeOptions that can be used for all platforms."""
    options = webdriver.ChromeOptions()
    options.add_argument('disable-field-trial-config')

    # browserStartupTimeout is added at 117.0.5913.0
    if self.supports_startup_timeout:
      # The default timeout is 60 seconds, in case of crashes, this increases
      # the test time. Chrome should start in 10 seconds.
      options.add_experimental_option('browserStartupTimeout', 10000)
    return options

  def get_driver_session_folder(self, session_counter: int) -> str:
    folder = os.path.join(self.artifacts_path, f'session-{session_counter}')
    if not os.path.exists(folder):
      os.mkdir(folder)
    return folder

  def get_driver_service(self) -> service.Service:
    """The Service to start Chrome."""
    self._driver_session_counter += 1
    driver_log = os.path.join(
      self.get_driver_session_folder(self.driver_session_counter),
      'driver.log')
    service_args=[
      # Skipping Chrome version check, this allows chromdriver to communicate
      # with Chrome of more than two versions older.
      '--disable-build-check',
      '--enable-chrome-logs',
      f'--log-path={driver_log}']
    return ChromeService(self.chromedriver_path, service_args=service_args)

  def wait_for_window(self,
                      driver: webdriver.Remote,
                      timeout: float = DEFAULT_WAIT_TIMEOUT):
    """Waits for the window handle to be available."""
    start_time = time.time()
    while time.time() - start_time <= timeout:
      try:
        driver.current_window_handle
        return
      except WebDriverException:
        logging.info('continue to wait on window handles.')
        time.sleep(DEFAULT_WAIT_INTERVAL)
    raise RuntimeError('Failed to get window handles.')

  @contextmanager
  def create_driver(self,
                    seed_file: Optional[str] = None,
                    options: Optional[webdriver.ChromeOptions] = None
    ) -> webdriver.Remote:
    """Creates a webdriver.

    Args:
      seed_file: the path to the seed file for Chrome to launch with.
      options: the ChromeOptions to launch the Chrome that applies to all
               platforms. If None, the default_options is used.

    Returns:
      An instance of webdriver.Remote
    """
    raise NotImplemented

  def close(self):
    """Cleans up anything that is created during the session."""
    pass
