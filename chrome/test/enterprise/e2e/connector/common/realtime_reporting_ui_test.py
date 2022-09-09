# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
from absl import app
from pywinauto.application import Application
from selenium import webdriver

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot

UnsafePageLink = "http://testsafebrowsing.appspot.com/s/malware.html"
UnsafeDownloadLink = "http://testsafebrowsing.appspot.com/s/badrep.exe"


def visit(window, url):
  """Visit a specific URL through pywinauto.Application.

  SafeBrowsing intercepts HTTP requests & hangs WebDriver.get(), which prevents
  us from getting the page source. Using pywinauto to visit the pages instead.
  """
  window.Edit.set_edit_text(url).type_keys("%{ENTER}")
  time.sleep(10)


def main(argv):
  exclude_switches = ["disable-background-networking"]
  chrome_options = webdriver.ChromeOptions()
  chrome_options.add_experimental_option("excludeSwitches", exclude_switches)

  driver = create_chrome_webdriver(chrome_options=chrome_options)

  try:
    app = Application(backend="uia")
    app.connect(title_re='.*Chrome|.*Chromium')
    window = app.top_window()

    # Wait for Chrome to download SafeBrowsing lists in the background.
    # There's no trigger to force this operation or synchronize on it, but quick
    # experiments have shown 3-4 minutes in most cases, so 5 should be plenty.
    time.sleep(60 * 5)

    # Verify Policy status legend in chrome://policy page
    policy_url = "chrome://policy"
    driver.get(policy_url)
    driver.find_element_by_id('reload-policies').click
    # Give the page 2 seconds to render the legend
    time.sleep(2)
    status_box = driver.find_element_by_css_selector("status-box")
    el = getElementFromShadowRoot(driver, status_box, "fieldset")

    deviceId = el.find_element_by_class_name(
        'machine-enrollment-device-id').text

    visit(window, UnsafePageLink)

    visit(window, UnsafeDownloadLink)
    print('\nDeviceId:' + deviceId.strip())

  except Exception as error:
    print(error)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
