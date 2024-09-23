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

AUTH_URL = (
    r"https://login.microsoftonline.com/organizations/"
    r"oauth2/v2.0/authorize?"
    r"client_id=bfb3dc57-d2b9-4b83-9a1d-7bf987e115fd&"
    r"redirect_uri=https://chromeenterprise.google/enroll/&"
    r"response_type=token%20id_token&"
    r"scope=openid+email+profile&"
    r"nonce=072f41d79a3cda30b589143eba6cd479140aa51c545f813365f839b4967d0347&"
    r"prompt=select_account")


def enroll(driver):
  # Go to MSFT auth URL
  driver.get(AUTH_URL)
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
        title="Continue", control_type="Button").click()  # Confirm the dialogue
    time.sleep(10)  # Wait for the completion of work profile enrollment
    window.child_window(title="Confirm", control_type="Button").click()
    time.sleep(5)

    # Obtain the workprofile's UI window
    workprofile = app.top_window()

    # Check user identity status in the profile menu
    if "demo" in FLAGS.account:
      workprofile.child_window(
          title="Demo Test (Work)", control_type="Button").click()
      if workprofile.child_window(
          title=FLAGS.account, control_type="Text").exists():
        logging.info("Icebreaker work profile created")

    if "enterprise" in FLAGS.account:
      workprofile.child_window(
          title="Enterprise Test (Work)", control_type="Button").click()
      if workprofile.child_window(
          title=FLAGS.account, control_type="Text").exists():
        logging.info("Dasherless work profile created")

    # Check chrome policy page to see the cloud policies
    driver.switch_to.window(driver.window_handles[0])
    driver.get('chrome://policy')
    driver.find_element(By.ID, 'reload-policies').click
    # Give the page 5 seconds to render the legend
    time.sleep(5)
    status_box = driver.find_elements(By.CSS_SELECTOR, "status-box")[0]

    el = getElementFromShadowRoot(driver, status_box, ".status-box-fields")

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
