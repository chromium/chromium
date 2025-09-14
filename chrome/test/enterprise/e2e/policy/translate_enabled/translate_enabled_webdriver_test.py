# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from absl import app, flags
from selenium import webdriver
from pywinauto.application import Application
from pywinauto.findwindows import ElementNotFoundError

from test_util import create_chrome_webdriver, fetch_policies

# A URL that is in a different language than our Chrome language.
URL = "https://zh.wikipedia.org/wiki/Chromium"

FLAGS = flags.FLAGS

flags.DEFINE_bool('incognito', False,
                  'Set flag to open Chrome in incognito mode.')


def main(argv):
  options = webdriver.ChromeOptions()
  # By default, `chromedriver` sets a pref to disable translation. Override
  # that so that the policy alone decides the feature's enablement.
  prefs = {"translate": {"enabled": True}}
  driver = create_chrome_webdriver(
      chrome_options=options, incognito=FLAGS.incognito, prefs=prefs)

  fetch_policies(driver)
  driver.get(URL)
  # Wait for UI to update
  time.sleep(10)
  translatePopupVisible = None

  try:
    app = Application(backend="uia")
    app.connect(title_re='.*Chrome|.*Chromium')
    # Use more specific criteria to avoid ambiguity
    translate_pane = app.top_window().child_window(
        title="Translate this page?",
        control_type="Pane",
        found_index=0  # Take the first match
    )
    translate_pane.print_control_identifiers()
    translatePopupVisible = True
  except ElementNotFoundError:
    translatePopupVisible = False
  finally:
    driver.quit()

  if translatePopupVisible:
    print("TRUE")
  else:
    print("FALSE")


if __name__ == '__main__':
  app.run(main)
