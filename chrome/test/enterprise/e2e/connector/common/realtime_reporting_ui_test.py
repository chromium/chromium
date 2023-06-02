# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import json
import time

from absl import app
from pywinauto.application import Application
from selenium import webdriver
from selenium.webdriver.support.ui import WebDriverWait

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot

from histogram.util import poll_histogram

UnsafePageLink = 'http://testsafebrowsing.appspot.com/s/malware.html'
UnsafeDownloadLink = 'http://testsafebrowsing.appspot.com/s/badrep.exe'


def visit(window, url):
  """Visit a specific URL through pywinauto.Application.

  SafeBrowsing intercepts HTTP requests & hangs WebDriver.get(), which prevents
  us from getting the page source. Using pywinauto to visit the pages instead.
  """
  window.Edit.set_edit_text(url).type_keys('%{ENTER}')
  time.sleep(10)


def main(argv):
  exclude_switches = ['disable-background-networking']
  chrome_options = webdriver.ChromeOptions()
  chrome_options.add_experimental_option('excludeSwitches', exclude_switches)
  chrome_options.add_argument('--enable-stats-collection-bindings')

  result = {}
  driver = create_chrome_webdriver(chrome_options=chrome_options)

  try:
    app = Application(backend='uia')
    app.connect(title_re='.*Chrome|.*Chromium')
    window = app.top_window()

    # Wait for Chrome to download SafeBrowsing lists in the background.
    # There's no trigger to force this operation or synchronize on it, but quick
    # experiments have shown 3-4 minutes in most cases, so 5 should be plenty.
    time.sleep(60 * 5)
    # Open chrome://safe-browsing
    safe_browsing_url = 'chrome://safe-browsing'
    driver.get(safe_browsing_url)
    WebDriverWait(driver=driver, timeout=10)
    # navigate to "reporting events" tab
    driver.find_element_by_css_selector('#reporting').click()

    # Verify Policy status legend in chrome://policy page
    # Switch to the new window
    policy_url = 'chrome://policy'
    logging.info('Navigating to chrome://policy')
    # use visit so it will open policy app in a different tab
    visit(window, policy_url)
    driver.switch_to.window(driver.window_handles[1])
    WebDriverWait(driver=driver, timeout=10)

    driver.find_element_by_xpath('//*[@id="reload-policies"]').click()
    # Give the page 2 seconds to render the legend
    time.sleep(2)
    status_box = driver.find_element_by_css_selector('status-box')
    el = getElementFromShadowRoot(driver, status_box, 'fieldset')

    deviceId = el.find_element_by_class_name(
        'machine-enrollment-device-id').text

    logging.info('Navigating to %s' % UnsafePageLink)
    visit(window, UnsafePageLink)

    logging.info('Navigating to %s' % UnsafeDownloadLink)
    visit(window, UnsafeDownloadLink)

    # print events logged at safe-browsing app
    driver.switch_to.window(driver.window_handles[0])
    WebDriverWait(driver=driver, timeout=10)
    msgs = json.loads(
        driver.find_element_by_css_selector('#reporting-events > div').text)
    result['ReportedEvent'] = msgs
    result['DeviceId'] = deviceId.strip()

    hg = poll_histogram(driver, [
        'Enterprise.ReportingEventUploadSuccess',
        'Enterprise.ReportingEventUploadFailure',
    ])
    if hg:
      result['Histogram'] = hg

  except Exception as e:
    logging.critical(e, exc_info=True)
  finally:
    driver.quit()
  logging.info(f'Result:{json.dumps(result)}')


if __name__ == '__main__':
  app.run(main)
