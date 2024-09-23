# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from absl import app, flags
from selenium import webdriver
from selenium.common.exceptions import NoSuchElementException
from selenium.webdriver.common.by import By
from pywinauto.application import Application
from pywinauto.findwindows import ElementNotFoundError

from test_util import create_chrome_webdriver

FLAGS = flags.FLAGS

flags.DEFINE_string('url', None, 'The url to open in Chrome.')
flags.mark_flag_as_required('url')

def main(argv):
  chrome_options = webdriver.ChromeOptions()
  chrome_options.add_argument("--force-renderer-accessibility")
  #Always set useAutomationExtension as false to avoid failing launch Chrome
  #https://bugs.chromium.org/p/chromedriver/issues/detail?id=2930
  chrome_options.add_experimental_option("useAutomationExtension", False)
  driver = create_chrome_webdriver(chrome_options=chrome_options)
  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')
  driver.get(FLAGS.url)
  time.sleep(5)

  try:
    driver.find_element(By.XPATH,
                        "//div[@aria-label='Blocked by admin']").click()
    app.top_window() \
       .child_window(title_re='.*Your admin has blocked',
                      control_type="TitleBar") \
       .print_control_identifiers()
  except NoSuchElementException:
    print("Not blocked")
  except ElementNotFoundError:
    print("Not blocked")
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
