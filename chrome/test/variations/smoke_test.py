# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pytest

from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver import ChromeOptions
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.wait import WebDriverWait

from typing import Callable
from http.server import HTTPServer

from fixtures.skia_gold import VariationsSkiaGoldUtil

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))
TEST_DATA_DIR = os.path.join(SRC_DIR, 'chrome', 'test', 'data', 'variations')
DriverFactory = Callable[[ChromeOptions], webdriver.Remote]

def test_basic_rendering(driver_factory: DriverFactory,
                         local_http_server: HTTPServer,
                         skia_gold_util: VariationsSkiaGoldUtil):
  url = (f'http://localhost:{local_http_server.server_port}')
  with driver_factory() as driver:
    driver.set_window_size(800, 600)
    driver.get(url)
    body = WebDriverWait(driver, 5).until(
      EC.presence_of_element_located((By.TAG_NAME, 'body')))

    status, error_msg = skia_gold_util.compare(
      name='body',
      png_data=skia_gold_util.screenshot_from_element(body))

    assert status == 0, error_msg

def test_load_crash_seed(driver_factory: DriverFactory,
                         local_http_server: HTTPServer):
  seed_path = os.path.join(TEST_DATA_DIR, 'crash_seed.json')
  assert os.path.exists(seed_path)

  url = (f'http://localhost:{local_http_server.server_port}')
  chrome_options = ChromeOptions()
  chrome_options.add_argument(f'variations-test-seed-path={seed_path}')

  # Launch Chrome normally.
  with driver_factory() as driver:
    driver.get("chrome://version")
    version = WebDriverWait(driver, 5).until(
      EC.presence_of_element_located((By.ID, 'version')))
    logging.info('Chrome version: %s', version.text)

  # Launch again with bad seed, expecting a crash.
  with pytest.raises(WebDriverException):
    with driver_factory(chrome_options) as driver:
      driver.get(url)
