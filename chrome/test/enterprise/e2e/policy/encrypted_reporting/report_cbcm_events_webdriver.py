# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from absl import app
import time
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions
from selenium.webdriver.support.ui import WebDriverWait

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot


def main(argv):
  options = webdriver.ChromeOptions()
  os.environ['CHROME_LOG_FILE'] = r"C:\temp\chrome_log.txt"

  # This flag tells Chrome to send heartbeat events on start up.
  options.add_argument(
      '--enable-features=EncryptedReportingManualTestHeartbeatEvent,EncryptedReportingPipeline'
  )

  driver = create_chrome_webdriver(chrome_options=options)

  try:
    wait = WebDriverWait(driver, 10)
    policy_url = 'chrome://policy'
    driver.get(policy_url)

    wait.until(
        expected_conditions.visibility_of_element_located((By.ID, 'reload-policies'))
    )

    # Reload policies
    driver.find_element(By.ID, 'reload-policies').click

    wait.until(
        expected_conditions.visibility_of_element_located((By.CSS_SELECTOR, 'status-box'))
    )
    status_box = driver.find_element(By.CSS_SELECTOR, 'status-box')

    el = getElementFromShadowRoot(driver, status_box, '.status-box-fields')

    # Verify policy status legend in chrome://policy page
    print(el.find_element(By.CLASS_NAME, 'status-box-heading').text)
    print(el.find_element(By.CLASS_NAME, 'machine-enrollment-name').text)
    print(el.find_element(By.CLASS_NAME, 'machine-enrollment-token').text)
    print(el.find_element(By.CLASS_NAME, 'status').text)
    device_id = el.find_element(By.CLASS_NAME,
                                'machine-enrollment-device-id').text
    print('DEVICE_ID=' + device_id.strip())
  except Exception as error:
    print(error)
  finally:
    # Give the browser some time to send heartbeat events.
    time.sleep(20)

    # Print CHROME_LOG_FILE
    print('PRINTING CHROME LOG FILE....')
    with open(os.environ['CHROME_LOG_FILE']) as file:
      content = file.read()
      print(content)
    print('DONE PRINTING CHROME LOG FILE.')

    driver.quit()


if __name__ == '__main__':
  app.run(main)
