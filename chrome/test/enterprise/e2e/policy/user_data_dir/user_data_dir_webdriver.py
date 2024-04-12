# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from absl import app, flags
from selenium import webdriver
from selenium.webdriver.common.by import By
from test_util import create_chrome_webdriver

FLAGS = flags.FLAGS
flags.DEFINE_string('user_data_dir', None, 'Need specify user data dir to test')
flags.mark_flag_as_required('user_data_dir')


def main(argv):
  policy_url = "chrome://policy"
  version_url = "chrome://version"

  # Verify the user data dir is not existing before launch the Chrome
  print("User data before running chrome is " + str(
      os.path.isdir(FLAGS.user_data_dir)))

  # Launch real Chrome
  os.system('start chrome --remote-debugging-port=9222')

  options = webdriver.ChromeOptions()
  # Add option for connecting chromedriver with Chrome
  options.add_experimental_option("debuggerAddress", "localhost:9222")

  driver = create_chrome_webdriver(chrome_options=options)

  try:
    # Verify User Data Dir in chrome://policy page
    driver.get(policy_url)
    print(driver.find_element(By.CSS_SELECTOR, 'html').text.encode('utf-8'))

    # Verfiy User Data Dir used in chrome://version
    driver.get(version_url)
    print("Profile path is " + driver.find_element(By.ID, "profile_path").text)

    # Verify if UserDataDir folder is created
    print("User data dir creation is " + str(os.path.isdir(FLAGS.user_data_dir)))
  except Exception as error:
    print(error)
  finally:
    driver.quit()
    os.system('taskkill /f /im chrome.exe')


if __name__ == '__main__':
  app.run(main)
