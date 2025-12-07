# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import time

from absl import app, flags
from selenium import webdriver
from selenium.webdriver.common.by import By

from test_util import create_chrome_webdriver, sign_in

FLAGS = flags.FLAGS
flags.DEFINE_string(
    'account', None,
    'Sign into the browser as this account before refreshing policies')
flags.DEFINE_string('password', None, 'Account password')
flags.DEFINE_string('results', r'c:\temp\results.json',
                    'Path to write results to.')


def main(argv):
  options = webdriver.ChromeOptions()
  # Expose Chrome UI elements to `pywinauto`.
  options.add_argument('--force-renderer-accessibility')
  # Bypass a basic CAPTCHA by not identifying as a WebDriver-controlled browser
  # (i.e., sets `navigator.webdriver` to false).
  options.add_argument('--disable-blink-features=AutomationControlled')
  # Override chromedriver's default of disabling sync/sign-in.
  options.add_experimental_option('excludeSwitches', ['disable-sync'])
  os.environ['CHROME_LOG_FILE'] = r'C:\temp\chrome_log.txt'

  driver = create_chrome_webdriver(chrome_options=options)
  # Wait up to 10s for finding elements.
  driver.implicitly_wait(10)

  try:
    if FLAGS.account and FLAGS.password:
      sign_in(driver, FLAGS.account, FLAGS.password)

      # Verify Policy status legend in chrome://policy page.
      policy_url = 'chrome://policy'
      driver.get(policy_url)

      # Give the page 10 seconds for `enrollment and legend rending.
      time.sleep(10)
      driver.find_element(By.ID, 'reload-policies').click()
      # Wait for policy fetch to complete.
      time.sleep(10)

    # The JSON content on chrome://prefs-internals is typically within a <pre>
    # tag.
    driver.get('chrome://prefs-internals')
    try:
      pre_element = driver.find_element(By.TAG_NAME, 'pre')
      json_text = pre_element.text
    except Exception as e:
      print(f"Error finding <pre> tag or getting its text: {e}")
      raise

    # Parse the JSON data from the page source.
    try:
      prefs_data = json.loads(json_text)
    except json.JSONDecodeError as e:
      print(f"Error decoding JSON: {e}")
      print(f"JSON text was: {json_text}")
      raise

    # Write data to assert in test.
    with open(FLAGS.results, 'w+') as results_file:
      # Ensure 'browser' key exists before attempting to write.
      try:
        json.dump(prefs_data['browser']['gemini_settings'], results_file)
      except KeyError:
        print("Warning: 'browser' or 'gemini_settings' key not found in "
              "prefs_data. Output file will be empty.")
        json.dump({}, results_file)

  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
