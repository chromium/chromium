# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from absl import app
from selenium.webdriver.common.by import By
from test_util import create_chrome_webdriver
from test_util import getElementFromShadowRoot


def main(argv):
  driver = create_chrome_webdriver()
  driver.get("chrome://password-manager/passwords")

  # The settings is nested within multiple shadow doms - extract it.
  selectors = ["settings-section", "#passwordToggle", "cr-toggle"]

  el = driver.find_element(By.TAG_NAME, "password-manager-app")
  for selector in selectors:
    el = getElementFromShadowRoot(driver, el, selector)

  if el.get_attribute("checked"):
    print("TRUE")
  else:
    print("FALSE")

  driver.quit()


if __name__ == '__main__':
  app.run(main)
