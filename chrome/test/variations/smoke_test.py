# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pytest

from chrome.test.variations import drivers
from chrome.test.variations import fixtures
from chrome.test.variations.fixtures import test_options
from chrome.test.variations.test_utils.driver import DriverUtil
from http.server import HTTPServer
from selenium.common.exceptions import WebDriverException
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.wait import WebDriverWait


def test_load_simple_url(driver_factory: drivers.DriverFactory,
                         local_http_server: HTTPServer,
                         seed_locator: fixtures.SeedLocator,
                         test_options: test_options.TestOptions,
                         add_tag: fixtures.result_sink.AddTag,
                         add_features: fixtures.features.AddFeatures):
  url = (f'http://localhost:{local_http_server.server_port}')
  with driver_factory.create_driver(
    seed_file=seed_locator.get_seed()) as driver:
    versions = driver.execute_cdp_cmd(cmd='Browser.getVersion', cmd_args={})
    # https://chromedevtools.github.io/devtools-protocol/tot/Browser/#method-getVersion
    # expecting { 'product': 'Chrome/120.0.6090.0' }
    version = versions['product'].split('/')[1]
    add_tag('launched_version', version)
    driver.set_window_size(800, 600)
    driver.get(url)
    WebDriverWait(driver, 5).until(
      EC.presence_of_element_located((By.TAG_NAME, 'body')))

    # log features
    features = DriverUtil(driver, test_options).get_features()
    add_features(features)


def test_basic_rendering(driver_factory: drivers.DriverFactory,
                         local_http_server: HTTPServer,
                         seed_locator: fixtures.SeedLocator,
                         test_options: test_options.TestOptions,
                         skia_gold_util: fixtures.VariationsSkiaGoldUtil,
                         add_features: fixtures.features.AddFeatures):
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

    # log features
    features = DriverUtil(driver, test_options).get_features()
    add_features(features)


def test_load_crash_seed(driver_factory: drivers.DriverFactory,
                         local_http_server: HTTPServer,
                         seed_locator: fixtures.SeedLocator):
  url = (f'http://localhost:{local_http_server.server_port}')
  # Launch Chrome normally.
  with driver_factory.create_driver() as driver:
    driver.get(url)
    WebDriverWait(driver, 5).until(
      EC.presence_of_element_located((By.TAG_NAME, 'body')))

  # Launch again with bad seed, expecting a crash.
  with pytest.raises(WebDriverException):
    with driver_factory.create_driver(
      seed_file=seed_locator.get_seed(fixtures.SeedName.CRASH)
      ) as driver:
      driver.get(url)
