# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

from absl import app, flags
from selenium import webdriver
from selenium.webdriver.common.by import By

from test_util import create_chrome_webdriver, sign_in
from test_util import getElementFromShadowRoot
from test_util import getElementsFromShadowRoot

FLAGS = flags.FLAGS
flags.DEFINE_string(
    "account", None,
    "Sign into the browser as this account before refreshing policies")
flags.DEFINE_string("password", None, "Account password")

_POLICY_CHROME_DATA_REGION_SETTING = "ChromeDataRegionSetting"


def main(argv):
  options = webdriver.ChromeOptions()
  # Expose Chrome UI elements to `pywinauto`.
  options.add_argument("--force-renderer-accessibility")
  # Bypass a basic CAPTCHA by not identifying as a WebDriver-controlled browser
  # (i.e., sets `navigator.webdriver` to false).
  options.add_argument("--disable-blink-features=AutomationControlled")
  # Override chromedriver's default of disabling sync/sign-in.
  options.add_experimental_option("excludeSwitches", ["disable-sync"])
  os.environ["CHROME_LOG_FILE"] = r"C:\temp\chrome_log.txt"

  driver = create_chrome_webdriver(chrome_options=options)
  # Wait up to 10s for finding elements.
  driver.implicitly_wait(10)

  try:
    if FLAGS.account and FLAGS.password:
      sign_in(driver, FLAGS.account, FLAGS.password)

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

  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
