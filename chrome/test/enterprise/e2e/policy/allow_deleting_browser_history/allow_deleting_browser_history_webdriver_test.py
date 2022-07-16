# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import app
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions
from selenium.webdriver.support.ui import WebDriverWait

import test_util

# Detect if history deletion is enabled or disabled and print the result.

# The way to check is:
# - visit chrome://history;
# - get the first history item;
# - check the checkbox. If history deletion is disabled, then the check
#   box has attribute 'disabled';

# TODO(crbug.com/986444): move those helper methods into test_util.py once
def getElementFromShadowRoot(driver, element, selector):
  if element is None:
    return None
  else:
    return driver.execute_script(
        "return arguments[0].shadowRoot.querySelector(arguments[1])", element,
        selector)


def main(argv):
  driver = test_util.create_chrome_webdriver()

  try:
    driver.get('http://www.google.com')
    driver.get('chrome://history')

    # wait for page to be loaded
    wait = WebDriverWait(driver, 10)
    wait.until(
        expected_conditions.visibility_of_element_located((By.TAG_NAME,
                                                           'history-app')))

    history_app = driver.find_element_by_css_selector("history-app")
    histroy_list = getElementFromShadowRoot(driver, history_app, "history-list")
    # get the checkbox of the first history item
    histroy_item = getElementFromShadowRoot(driver, histroy_list,
                                            'history-item')
    checkbox = getElementFromShadowRoot(driver, histroy_item,
                                        '#main-container cr-checkbox')
    disabled = checkbox.get_attribute('disabled')
    if disabled == 'true':
      print('DISABLED')
    else:
      print('ENABLED')

  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
