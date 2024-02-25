# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait

from test_util import create_chrome_webdriver, getElementFromShadowRoot

driver = create_chrome_webdriver()
driver.get('chrome://policy')

# Wait until the various policy tables are loaded.
shadow_root_wrapper = WebDriverWait(driver, 10).until(
    EC.presence_of_all_elements_located(
        (By.CSS_SELECTOR, "#main-section > policy-table")))

# Get the element containing the precedence order which is nested under two
# shadow roots.
precedence_row = getElementFromShadowRoot(driver, shadow_root_wrapper[1],
                                          "policy-precedence-row")
precedence_value = getElementFromShadowRoot(driver, precedence_row,
                                            ".precedence > .value")

print(precedence_value.text)

driver.quit()
