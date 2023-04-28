# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pytest

from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver import ChromeOptions
from typing import Callable
from http.server import HTTPServer

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))
TEST_DATA_DIR = os.path.join(SRC_DIR, 'chrome', 'test', 'data', 'variations')
DriverFactory = Callable[[ChromeOptions], webdriver.Remote]

def test_load_crash_seed(driver_factory: DriverFactory,
                         local_http_server: HTTPServer):
  seed_path = os.path.join(TEST_DATA_DIR, 'crash_seed.json')
  assert os.path.exists(seed_path)

  url = (f'http://localhost:{local_http_server.server_port}')
  chrome_options = ChromeOptions()
  chrome_options.add_argument(f'variations-test-seed-path={seed_path}')

  # Launch Chrome normally.
  with driver_factory() as driver:
    driver.get(url)

  # Launch again with bad seed, expecting a crash.
  with pytest.raises(WebDriverException):
    with driver_factory(chrome_options) as driver:
      driver.get(url)
