# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions for extracting features."""

import dataclasses
import html
import logging
import os
import sys

from typing import List

from chrome.test.variations.fixtures import features
from chrome.test.variations.fixtures import test_options

from selenium import webdriver
from selenium.common.exceptions import NoSuchElementException
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.wait import WebDriverWait


from chrome.test.variations.test_utils import SRC_DIR
sys.path.append(os.path.join(SRC_DIR, 'tools'))
from variations.split_variations_cmd import ParseVariationsCmdFromString

_GET_VERSIONS_COMMAND_URL = 'chrome://version/?show-variations-cmd'


def _extract_features_from_commandline(
  commandline_html: str) -> features.Features:
  # Remove flag which is not handled by split_variations_cmd.
  text = html.unescape(commandline_html).replace(
    '--disable-field-trial-config', '')
  logging.info('Parsing variations command line data')
  switches_dict = ParseVariationsCmdFromString(text)
  return features.Features(
    enabled=[
        feature.key for feature in switches_dict.get('enable-features', [])
    ],
    disabled=[
        feature.key for feature in switches_dict.get('disable-features', [])
    ],
  )



class DriverUtil:
  def __init__(
    self, driver: webdriver.Remote, test_options: test_options.TestOptions):
    self._driver = driver
    self._test_options = test_options

  def get_features(self) -> features.Features:
    # Cannot use the chrome version page on webview.
    if self._test_options.platform in ['webview', 'android_webview']:
      logging.info('Skipping feature logging on webview')
      return features.Features([], [])
    logging.info('Fetching data from chrome://version/?show-variations-cmd')
    self._driver.get(_GET_VERSIONS_COMMAND_URL)
    WebDriverWait(self._driver, 5).until(
      EC.presence_of_element_located((By.TAG_NAME, 'body')))
    try:
      command_element = self._driver.find_element(By.ID, "variations-cmd")
    except NoSuchElementException:
      logging.warning(
        'Cannot get features: error in getting variations command')
      return features.Features([], [])
    command_line_text = command_element.get_attribute('innerHTML')
    return _extract_features_from_commandline(command_line_text)
