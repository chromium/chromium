# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from absl import app
import time
from selenium import webdriver

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot


def main(argv):
  options = webdriver.ChromeOptions()
  os.environ["CHROME_LOG_FILE"] = r"c:\temp\chrome_log.txt"
  driver = create_chrome_webdriver(chrome_options=options)

  # Give some time for browser to enroll
  time.sleep(10)

  try:
    # Verify Policy status legend in chrome://policy page
    policy_url = "chrome://policy"
    driver.get(policy_url)
    driver.find_element_by_id('reload-policies').click
    # Give the page 2 seconds to render the legend
    time.sleep(2)
    status_box = driver.find_element_by_css_selector("status-box")
    el = getElementFromShadowRoot(driver, status_box, "fieldset")

    print(el.find_element_by_class_name('legend').text)
    print(el.find_element_by_class_name('machine-enrollment-name').text)
    print(el.find_element_by_class_name('machine-enrollment-token').text)
    print(el.find_element_by_class_name('status').text)
  except Exception as error:
    print(error)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
