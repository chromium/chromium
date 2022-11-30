# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import time

from absl import app, flags
from selenium import webdriver
from pywinauto.application import Application
from pywinauto.findwindows import ElementNotFoundError

from test_util import create_chrome_webdriver

# A URL that is in a different language than our Chrome language.
URL = "https://zh.wikipedia.org/wiki/Chromium"

FLAGS = flags.FLAGS

flags.DEFINE_bool('incognito', False,
                  'Set flag to open Chrome in incognito mode.')


def main(argv):
  os.system('start chrome --remote-debugging-port=9222')
  options = webdriver.ChromeOptions()
  # Add option for connecting chromedriver with Chrome
  options.add_experimental_option("debuggerAddress", "localhost:9222")
  driver = create_chrome_webdriver(
      chrome_options=options, incognito=FLAGS.incognito)
  driver.get(URL)
  time.sleep(10)
  # Refresh the tab so the TranslateEnabled policy can apply
  driver.refresh()
  translatePopupVisible = None

  try:
    app = Application(backend="uia")
    app.connect(title_re='.*Chrome|.*Chromium')
    app.top_window() \
       .child_window(title="Translate this page?", control_type="Pane") \
       .print_control_identifiers()
    translatePopupVisible = True
  except ElementNotFoundError as error:
    translatePopupVisible = False
  finally:
    driver.quit()
    os.system('taskkill /f /im chrome.exe')

  if translatePopupVisible:
    print("TRUE")
  else:
    print("FALSE")


if __name__ == '__main__':
  app.run(main)
