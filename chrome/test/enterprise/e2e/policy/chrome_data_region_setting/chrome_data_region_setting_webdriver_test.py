# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from absl import app
import time
from selenium import webdriver
from selenium.webdriver.common.by import By

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot
from test_util import getElementsFromShadowRoot

_POLICY_CHROME_DATA_REGION_SETTING = "ChromeDataRegionSetting"


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

    policy_table = driver.find_element(By.CSS_SELECTOR, "policy-table")
    row_groups = getElementsFromShadowRoot(driver, policy_table, '.policy-data')

    for group in row_groups:
      name = getElementFromShadowRoot(driver, group, '#name').text
      if not name:
        break
      if name == _POLICY_CHROME_DATA_REGION_SETTING:
        print("value=" + getElementFromShadowRoot(
            driver, group, 'div.policy.row > div.value').text)
        print("source=" + getElementFromShadowRoot(
            driver, group, 'div.policy.row > div.source').text)
        print("scope=" + getElementFromShadowRoot(
            driver, group, 'div.policy.row > div.scope').text)
        print("status=" + getElementFromShadowRoot(
            driver, group, 'div.policy.row > div.messages').text)
        break

  except Exception as error:
    print(error)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
