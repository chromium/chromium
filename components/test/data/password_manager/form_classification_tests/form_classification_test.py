# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The base class for unittest classes for testing form classification.
"""

import os.path
import time
import unittest

from selenium import webdriver
from selenium.common.exceptions import TimeoutException
from selenium.webdriver import ActionChains
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait

# Path to Chrome binary (the filename should be included).
PATH_TO_CHROME = "path/to/chrome_binary"
# Path to Chrome Driver binary (the filename should be included).
PATH_TO_CHROMEDRIVER = "path/to/chromedriver_binary"

# The form classifier in Chrome adds a mark attribute to the fields where
# password generation should be offered.
MARK_ATTRIBUTE_NAME = "pm_debug_pwd_creation_field"

# Timeout in seconds that Chrome Driver waits before raising an exception if
# an element is inaccessible.
# Since some sites have huge latency, the value is so large.
ELEMENTS_ACCESS_TIMEOUT = 10

# Timeout in seconds that Chrome Driver waits before checking outcome of the
# form classifier in Chrome.
CLASSIFIER_TIMEOUT = 2


class FormClassificationTest(unittest.TestCase):

  @classmethod
  def setUpClass(cls):
    if not os.path.isfile(PATH_TO_CHROME):
      raise Exception("No Chrome binary at '" + PATH_TO_CHROME + "'. "
                      "Set PATH_TO_CHROME.")
    if not os.path.isfile(PATH_TO_CHROMEDRIVER):
      raise Exception("No Chrome Driver binary at '" +
                      PATH_TO_CHROMEDRIVER + "'."
                      "Set PATH_TO_CHROMEDRIVER.")
    options = Options()
    options.binary_location = PATH_TO_CHROME
    FormClassificationTest.driver = webdriver.Chrome(PATH_TO_CHROMEDRIVER,
                                                     chrome_options=options)

  @classmethod
  def tearDownClass(cls):
    FormClassificationTest.driver.quit()

  def GetNoElementMessage(self, element_type, selector):
    return ("No " + element_type + " with CSS selector '" +  selector +
            "' was found.")

  def Click(self, selector):
    """Clicks on the element described by |selector|.

    Args:
      selector: The clicked element's CSS selector.
    """

    try:
      element = WebDriverWait(
          FormClassificationTest.driver, ELEMENTS_ACCESS_TIMEOUT).until(
              EC.element_to_be_clickable((By.CSS_SELECTOR, selector)))
    except TimeoutException as e:
      e.msg = self.GetNoElementMessage("clickable element", selector)
      raise
    action = ActionChains(FormClassificationTest.driver)
    action.move_to_element(element).click(element).perform()

  def GoTo(self, url):
    """Navigates the main frame to |url|.

    Args:
      url: The URL of where to go to.
    """

    FormClassificationTest.driver.get(url)
    FormClassificationTest.driver.delete_all_cookies()

  def SwitchTo(self, selector):
    """Switch to frame described by |selector|.

    Args:
      selector: The frame's CSS selector.
    """

    try:
      WebDriverWait(
          FormClassificationTest.driver, ELEMENTS_ACCESS_TIMEOUT).until(
              EC.frame_to_be_available_and_switch_to_it(
                  (By.CSS_SELECTOR, selector)))
    except TimeoutException as e:
      e.msg = self.GetNoElementMessage("iframe", selector)
      raise

  def GetFailureMessage(self, selector, is_pwd_creation, failure_cause):
    if failure_cause:
      return "Known failure. Cause: " + failure_cause
    msg = "Chrome didn't classify '" + selector + "' as password "
    if is_pwd_creation:
      msg += "creation"
    else:
      msg += "usage"
    msg += " field."
    return msg

  def CheckPwdField(self, selector, failure_cause=None, is_pwd_creation=True):
    """Checks the client-side classifier's outcome.

    Check if the password field described by |selector| is marked as
    password creation or usage field (i.e. the classifier set the
    attribute of the password field to "1").
    |is_pwd_creation| is the expected outcome of the classifier.
    If actual result is differ, an assert error will be raised.

    Args:
      selector: The checked password element's CSS selector.
      failure_cause: The cause of failure for the given test, if it is known.
      is_pwd_creation: The expected outcome of the classifier.
    """

    time.sleep(CLASSIFIER_TIMEOUT)  # Let classifier in Chrome do its work.
    try:
      field = WebDriverWait(
          FormClassificationTest.driver, ELEMENTS_ACCESS_TIMEOUT).until(
              EC.visibility_of_element_located((By.CSS_SELECTOR, selector)))
    except TimeoutException as e:
      e.msg = self.GetNoElementMessage("password element", selector)
      raise

    has_attribute = field.get_attribute(MARK_ATTRIBUTE_NAME) is not None
    assert has_attribute == is_pwd_creation, self.GetFailureMessage(
        selector, is_pwd_creation, failure_cause)
