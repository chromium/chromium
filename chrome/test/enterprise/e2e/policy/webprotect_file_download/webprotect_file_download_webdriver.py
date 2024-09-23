# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import time

from absl import app, flags
from selenium import webdriver
from selenium.webdriver.common.by import By
from pywinauto.application import Application
from pywinauto.findwindows import ElementNotFoundError

from test_util import create_chrome_webdriver

encrypted_file_url = "https://storage.googleapis.com/testfiles-webprotect/malware_file_encrypted.zip"
large_file_url = "https://storage.googleapis.com/testfiles-webprotect/large_file.bin"
unknown_malware_url = "https://storage.googleapis.com/testfiles-webprotect/malware_file.zip"

encrypted_block_text = ".* is\nencrypted\. Ask its owner to decrypt\."
large_block_text = ".* is too big for a security\ncheck\. You can open files up to 50 MB\."
unknown_malware_block_text = "Checking .* with your\norganization's security policies"


def download_file_from_url(driver, url, regex, wait, type, message):
  driver.get(url)
  time.sleep(wait)
  try:
    app = Application(backend="uia")
    app.connect(title_re='.*Chrome|.*Chromium')
    app.top_window() \
        .child_window(title_re=regex, control_type=type)
    print(message)
  except ElementNotFoundError as error:
    print(error)


def main(argv):
  exclude_switches = ["disable-background-networking"]
  options = webdriver.ChromeOptions()
  options.add_argument("--force-renderer-accessibility")
  options.add_experimental_option("excludeSwitches", exclude_switches)
  os.environ["CHROME_LOG_FILE"] = r"c:\temp\chrome_log.txt"
  driver = create_chrome_webdriver(chrome_options=options)

  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')
  # Wait for browser enrolling
  time.sleep(15)

  # Click reload policy to pull cloud policies from the server side
  policy_url = "chrome://policy"
  driver.get(policy_url)
  driver.find_element(By.ID, 'reload-policies').click

  download_file_from_url(driver, encrypted_file_url, encrypted_block_text, 30,
                         "Button", "Encrypted blocked")
  download_file_from_url(driver, large_file_url, large_block_text, 30, "Button",
                         "Large file blocked")
  download_file_from_url(driver, unknown_malware_url,
                         unknown_malware_block_text, 2, "Text",
                         "Unknown malware scanning")

  driver.quit()


if __name__ == '__main__':
  app.run(main)
