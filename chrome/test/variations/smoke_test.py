# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import pytest

from chrome.test.variations import drivers
from chrome.test.variations import fixtures
from http.server import HTTPServer
from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from selenium.webdriver import ChromeOptions
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.wait import WebDriverWait


def test_load_simple_url(driver_factory: drivers.DriverFactory,
                         local_http_server: HTTPServer,
                         seed_locator: fixtures.SeedLocator):
  url = (f'http://localhost:{local_http_server.server_port}')
  with driver_factory.create_driver(
    seed_file=seed_locator.get_seed()) as driver:
    driver.set_window_size(800, 600)
    driver.get(url)
    WebDriverWait(driver, 5).until(
      EC.presence_of_element_located((By.TAG_NAME, 'body')))


def test_basic_rendering(driver_factory: drivers.DriverFactory,
                         local_http_server: HTTPServer,
                         seed_locator: fixtures.SeedLocator,
                         skia_gold_util: fixtures.VariationsSkiaGoldUtil):
  url = (f'http://localhost:{local_http_server.server_port}')
  with driver_factory.create_driver(
    seed_file=seed_locator.get_seed()) as driver:
    driver.set_window_size(800, 600)
    driver.get(url)
    body = WebDriverWait(driver, 5).until(
      EC.presence_of_element_located((By.TAG_NAME, 'body')))

    status, error_msg = skia_gold_util.compare(
      name='body',
      png_data=skia_gold_util.screenshot_from_element(body))

    assert status == 0, error_msg

def test_load_crash_seed(driver_factory: drivers.DriverFactory,
                         local_http_server: HTTPServer,
                         seed_locator: fixtures.SeedLocator):
  url = (f'http://localhost:{local_http_server.server_port}')
  # Launch Chrome normally.
  with driver_factory.create_driver() as driver:
    driver.get(url)
    WebDriverWait(driver, 5).until(
      EC.presence_of_element_located((By.TAG_NAME, 'body')))

  # Expecting Chrome to crash, setting a shorter startup timeout.
  # The default is 60 seconds, changing to 5 seconds.
  options = ChromeOptions()
  options.set_capability('browserStartupTimeout', 5000)
  # Launch again with bad seed, expecting a crash.
  with pytest.raises(WebDriverException):
    with driver_factory.create_driver(
      seed_file=seed_locator.get_seed(fixtures.SeedName.CRASH),
      options = options) as driver:
      driver.get(url)
