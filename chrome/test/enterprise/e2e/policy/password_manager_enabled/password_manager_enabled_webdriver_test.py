# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import test_util
from absl import app


def getElementFromShadowRoot(driver, element, selector):
  if element is None:
    return None
  else:
    return driver.execute_script(
        "return arguments[0].shadowRoot.querySelector(arguments[1])", element,
        selector)


def main(argv):
  driver = test_util.create_chrome_webdriver()
  driver.get("chrome://settings/passwords")

  # The settings is nested within multiple shadow doms - extract it.
  selectors = [
      "settings-main", "settings-basic-page", "settings-autofill-page",
      "passwords-section", "#passwordToggle", "cr-toggle"
  ]

  el = driver.find_element_by_css_selector("settings-ui")
  for selector in selectors:
    el = getElementFromShadowRoot(driver, el, selector)

  if el.get_attribute("checked"):
    print("TRUE")
  else:
    print("FALSE")

  driver.quit()


if __name__ == '__main__':
  app.run(main)
