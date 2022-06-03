# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

from absl import app, flags
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from pywinauto.application import Application
import pyperclip

import test_util

FLAGS = flags.FLAGS

flags.DEFINE_string('text', None, 'The text to paste in form.')
flags.mark_flag_as_required('text')

paste_url = "https://storage.googleapis.com/testfiles-webprotect/bugbash/index.html"


def main(argv):
  os.system('start chrome --remote-debugging-port=9222')
  options = webdriver.ChromeOptions()
  options.add_argument("--force-renderer-accessibility")
  options.add_experimental_option("debuggerAddress", "localhost:9222")
  driver = test_util.create_chrome_webdriver(chrome_options=options)

  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')
  # Wait for browser enrolling
  time.sleep(15)

  # Click reload policy to pull cloud policies from the server side
  policy_url = "chrome://policy"
  driver.get(policy_url)
  driver.find_element_by_id('reload-policies').click

  # paste content from clipboard to the form
  driver.get(paste_url)
  form = driver.find_element_by_name('sensitive_data_scan')
  pyperclip.copy(FLAGS.text)
  form.send_keys(Keys.CONTROL, "v")
  time.sleep(20)

  try:
    # Print the UI elements which contain text
    for desc in app.top_window().descendants():
      print(desc.window_text())
    # Check if the paste is completed in form
    if form.get_attribute('value') == FLAGS.text:
      print("Paste is done")
    else:
      print("Paste is blocked")
  except:
    print("Error occurs")
  finally:
    driver.quit()
    os.system('taskkill /f /im chrome.exe')


if __name__ == '__main__':
  app.run(main)
