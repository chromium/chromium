# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from absl import app
import time
from selenium import webdriver
from selenium.webdriver.common.by import By

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot


def main(argv):
  options = webdriver.ChromeOptions()
  os.environ["CHROME_LOG_FILE"] = r"C:\temp\chrome_log.txt"

  driver = create_chrome_webdriver(chrome_options=options)

  try:
    # Verify Policy status legend in chrome://policy page
    policy_url = "chrome://policy"
    driver.get(policy_url)
    # Give the page 10 seconds for enrollment and legend rending
    time.sleep(10)
    driver.find_element(By.ID, 'reload-policies').click
    status_box = driver.find_element(By.CSS_SELECTOR, "status-box")
    el = getElementFromShadowRoot(driver, status_box, ".status-box-fields")

    print(el.find_element(By.CLASS_NAME, 'status-box-heading').text)
    print(el.find_element(By.CLASS_NAME, 'machine-enrollment-name').text)
    print(el.find_element(By.CLASS_NAME, 'machine-enrollment-token').text)
    print(el.find_element(By.CLASS_NAME, 'status').text)
    device_id = el.find_element(By.CLASS_NAME,
                                'machine-enrollment-device-id').text
    print("DEVICE_ID=" + device_id.strip())

    ## Upload a report and wait 5 seconds for the completion
    driver.find_element(By.ID, 'upload-report').click
    time.sleep(5)
  except Exception as error:
    print(error)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
