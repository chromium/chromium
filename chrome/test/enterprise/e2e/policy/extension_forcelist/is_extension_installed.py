# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import traceback
from absl import app, flags
from selenium import webdriver
from selenium.webdriver.common.by import By
from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot

FLAGS = flags.FLAGS

flags.DEFINE_string('extension_id', None,
                    'The id of the extension to look for.')
flags.mark_flag_as_required('extension_id')


def RunTest(driver):
  # The extension must be visible on the extensions page.
  driver.get("chrome://extensions")
  # It's nested within a couple of shadow doms on the page - extract it.
  print("Looking for extension on extensions page: %s" % FLAGS.extension_id)
  extension_page = False
  try:
    extension_manager_el = driver.find_element(By.CSS_SELECTOR,
                                               "extensions-manager")
    extension_item_list_el = getElementFromShadowRoot(driver,
                                                      extension_manager_el,
                                                      "extensions-item-list")
    extension_item_el = getElementFromShadowRoot(
        driver, extension_item_list_el,
        "extensions-item#%s" % FLAGS.extension_id)
    extension_page = (extension_item_el != None)
  except:
    print(traceback.format_exc())

  if extension_page:
    print("TRUE")
  else:
    print("FALSE")


def main(argv):
  try:
    chrome_options = webdriver.ChromeOptions()
    chrome_options.add_experimental_option("excludeSwitches",
                                           ["disable-background-networking"])

    driver = create_chrome_webdriver(chrome_options=chrome_options)

    # Wait for the extension to install on this new profile.
    time.sleep(20)

    RunTest(driver)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
