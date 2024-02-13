# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import attr
import logging
import os
import shutil

from contextlib import contextmanager
from typing import Optional

from chrome.test.variations.drivers import DriverFactory
from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver import ChromeOptions

@attr.attrs()
class DesktopDriverFactory(DriverFactory):
  """Driver factory for desktop platforms."""
  channel: Optional[str] = attr.attrib()
  crash_dump_dir: Optional[str] = attr.attrib()


  @contextmanager
  def create_driver(
    self,
    seed_file: Optional[str] = None,
    options: Optional[ChromeOptions] = None
    ) -> webdriver.Remote:
    os.environ['BREAKPAD_DUMP_LOCATION'] = self.crash_dump_dir

    options = options or self.default_options

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
      driver = webdriver.Chrome(service=self.get_driver_service(),
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

