# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from absl import app
from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot


def main(argv):
  driver = create_chrome_webdriver()
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
