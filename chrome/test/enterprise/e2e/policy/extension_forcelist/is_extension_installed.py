# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import test_util
import time
import traceback
from absl import app, flags
from selenium import webdriver

FLAGS = flags.FLAGS

flags.DEFINE_string('extension_id', None,
                    'The id of the extension to look for.')
flags.mark_flag_as_required('extension_id')


def getElementFromShadowRoot(driver, element, selector):
  if element is None:
    return None
  else:
    return driver.execute_script(
        "return arguments[0].shadowRoot.querySelector(arguments[1])", element,
        selector)


def RunTest(driver):
  # The extension must be visible on the extensions page.
  driver.get("chrome://extensions")
  # It's nested within a couple of shadow doms on the page - extract it.
  print("Looking for extension on extensions page: %s" % FLAGS.extension_id)
  extension_page = False
  try:
    extension_manager_el = driver.find_element_by_css_selector(
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

    driver = test_util.create_chrome_webdriver(chrome_options=chrome_options)

    # Wait for the extension to install on this new profile.
    time.sleep(20)

    RunTest(driver)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
