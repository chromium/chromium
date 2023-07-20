# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import time

from absl import app
from histogram.util import poll_histogram
from pywinauto.application import Application
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait
from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot


_UNSAFE_PAGE_LINK = 'http://testsafebrowsing.appspot.com/s/malware.html'
_UNSAFE_DOWNLOAD_LINK = 'http://testsafebrowsing.appspot.com/s/badrep.exe'
_TIMEOUT = 10


def visit(window, url):
  """Visit a specific URL through pywinauto.Application.

  SafeBrowsing intercepts HTTP requests & hangs WebDriver.get(), which prevents
  us from getting the page source. Using pywinauto to visit the pages instead.
  """
  window.Edit.set_edit_text(url).type_keys('%{ENTER}')
  time.sleep(10)


def wait_element(driver, by_selector, selector, timeout=_TIMEOUT * 3):
  return WebDriverWait(driver, timeout).until(
      EC.presence_of_element_located((by_selector, selector)),
      'Could not find element with selector: "{}"'.format(selector))


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

    # Verify Policy status legend in chrome://policy page
    # Switch to the new window
    policy_url = 'chrome://policy'
    logging.info('Navigating to chrome://policy')
    driver.get(policy_url)
    WebDriverWait(driver=driver, timeout=10)

    wait_element(driver, By.CSS_SELECTOR, '#reload-policies')
    driver.find_element(By.CSS_SELECTOR, '#reload-policies').click()
    # Give the page 2 seconds to render the legend
    time.sleep(2)
    wait_element(driver, By.CSS_SELECTOR, 'status-box')
    status_box = driver.find_element(By.CSS_SELECTOR, 'status-box')
    el = getElementFromShadowRoot(driver, status_box, '.status-box-fields')

    deviceId = el.find_element(By.CLASS_NAME,
                               'machine-enrollment-device-id').text

    logging.info('Navigating to %s' % _UNSAFE_PAGE_LINK)
    visit(window, _UNSAFE_PAGE_LINK)

    logging.info('Navigating to %s' % _UNSAFE_DOWNLOAD_LINK)
    visit(window, _UNSAFE_DOWNLOAD_LINK)

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
