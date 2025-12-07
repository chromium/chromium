# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import logging
import threading

from absl import app, flags
from pywinauto.application import Application
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot

FLAGS = flags.FLAGS
flags.DEFINE_string('account', None, 'The 3rd party account for authentication')
flags.DEFINE_string('password', None, 'The password')
flags.DEFINE_string('auth_url', None, 'The URL for profile registration.')

def enroll(driver):
  # Go to MSFT auth URL
  driver.get(FLAGS.auth_url)
  time.sleep(5)
  # Enter credentials
  driver.find_element(By.CSS_SELECTOR, '#i0116').send_keys(FLAGS.account)
  driver.find_element(By.CSS_SELECTOR, '#idSIButton9').click()
  time.sleep(5)
  driver.find_element(By.CSS_SELECTOR, '#i0118').send_keys(FLAGS.password)
  driver.find_element(By.CSS_SELECTOR, '#idSIButton9').click()
  time.sleep(5)
  driver.find_element(By.CSS_SELECTOR, '#idBtn_Back').click()


def main(argv):
  options = Options()
  # This flag enables the 3rd party managed profile feature
  options.add_argument("--enable-features=OidcAuthProfileManagement")
  # This flag allows pywinauto to access Chrome UI elements
  options.add_argument("--force-renderer-accessibility")
  driver = create_chrome_webdriver(chrome_options=options)

  try:
    app = Application(backend='uia')
    app.connect(title_re='.*Chrome|.*Chromium')
    window = app.top_window()

    thread = threading.Thread(target=enroll, args=(driver,))
    thread.start()

    time.sleep(30)  # Wait for the profile creation dialogue to show up

    window.child_window(
        title="Continue", auto_id="proceed-button",
        control_type="Button").click()  # Confirm the dialogue
    time.sleep(10)  # Wait for the completion of work profile enrollment
    window.child_window(
        title="Continue", auto_id="proceed-button",
        control_type="Button").click()
    time.sleep(5)

    # Obtain the workprofile's UI window
    workprofile = app.top_window()

    workprofile.child_window(
        title="Verify it's you", control_type="Button").click()
    # Check user identity status in the profile menu
    if "demo" in FLAGS.account:
      if workprofile.child_window(
          title="Demo Test • Work", control_type="Text").exists():
        logging.info("Icebreaker work profile created")

    if "enterprise" in FLAGS.account:
      if workprofile.child_window(
          title="Enterprise Test • Work", control_type="Text").exists():
        logging.info("Dasherless work profile created")

    # Check chrome policy page to see the cloud policies
    driver.switch_to.window(driver.window_handles[0])
    driver.get('chrome://policy')
    driver.find_element(By.ID, 'reload-policies').click
    # Give the page 5 seconds to render the legend
    time.sleep(5)
    # Loop through the status boxes as there might be more than one.
    for status_box in driver.find_elements(By.CSS_SELECTOR, "status-box"):
      el = getElementFromShadowRoot(driver, status_box, ".status-box-fields")
      logging.info("Found a status box")
      logging.info(el.find_element(By.CLASS_NAME, 'status-box-heading').text)
      logging.info(el.find_element(By.CLASS_NAME, 'username').text)
      logging.info(el.find_element(By.CLASS_NAME, 'profile-id').text)
      logging.info(el.find_element(By.CLASS_NAME, 'status').text)

  except Exception as e:
    logging.critical(e, exc_info=True)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
