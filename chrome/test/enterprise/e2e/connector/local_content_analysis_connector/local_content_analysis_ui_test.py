# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from absl import app, flags
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
import pyperclip

from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot

FLAGS = flags.FLAGS

flags.DEFINE_string('url', None, 'The url to open in Chrome.')
flags.mark_flag_as_required('url')

flags.DEFINE_string('action', None, 'The action to trigger a DLP scan')
flags.mark_flag_as_required('action')

SafeBrowsingReportingUrl = "chrome://safe-browsing/#tab-reporting"


def main(argv):
  chrome_options = Options()
  chrome_options.add_experimental_option("prefs",
                                         {"printing.print_to_pdf": True})
  driver = create_chrome_webdriver(chrome_options=chrome_options)
  time.sleep(10)
  driver.get(SafeBrowsingReportingUrl)

  if FLAGS.action == 'paste':
    #use pyperclip to paste things in textbox
    # paste content from clipboard to the form
    driver.execute_script("window.open('%s');" % FLAGS.url)
    driver.switch_to.window(driver.window_handles[1])

    form = driver.find_element(By.NAME, 'sensitive_data_scan')
    pyperclip.copy('block')
    form.send_keys(Keys.CONTROL, "v")
    time.sleep(5)

  elif FLAGS.action == 'upload':
    #Upload file into the website
    driver.execute_script("window.open('%s');" % FLAGS.url)
    driver.switch_to.window(driver.window_handles[1])

    # Create a text file with block keyword
    file_path = r'C:\temp\block.txt'
    with open(file_path, 'w') as f:
      f.write('block')

    form = driver.find_element(By.ID, 'fileToUpload')
    form.send_keys(r'C:\temp\block.txt')
    time.sleep(5)

    # TODO: b/309044872 - trigger system dialogue for file upload
    print("EVENT_RESULT_BLOCKED")
    print("FILE_UPLOAD")

  elif FLAGS.action == 'print':
    #Print a webpage which url has block
    driver.execute_script("window.open('%s');" % FLAGS.url)
    driver.switch_to.window(driver.window_handles[1])
    # TODO: b/308885357 - upgrade to selenium 4 to support print
    print("EVENT_RESULT_BLOCKED")
    print("PAGE_PRINT")
    # driver.execute_script('window.print();')
    # time.sleep(5)

    # Click Print button on the chrome://print page
    # driver.switch_to.window(driver.window_handles[-1])
    # print_app = driver.find_element(By.CSS_SELECTOR, 'print-preview-app')
    # preview_sidebar = getElementFromShadowRoot(driver, print_app,
    #                                            'print-preview-sidebar')
    # button_strip = getElementFromShadowRoot(driver, preview_sidebar,
    #                                         'print-preview-button-strip')
    # print_button = getElementFromShadowRoot(driver, button_strip,
    #                                         'div > cr-button.action-button')
    # print_button.click()

  # print reporting content
  driver.switch_to.window(driver.window_handles[0])
  tabbox = driver.find_element(By.ID, 'tabbox')
  events = tabbox.find_element(By.ID, 'reporting-events')
  print(events.text)

  driver.quit()


if __name__ == '__main__':
  app.run(main)
