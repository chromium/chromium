# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pywinauto.application import Application
from selenium import webdriver

from test_util import create_chrome_webdriver

# Set up ChromeDriver
options = webdriver.ChromeOptions()
options.add_argument("--force-renderer-accessibility")

driver = create_chrome_webdriver(chrome_options=options)

try:
  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')
  omnibox = app.top_window() \
            .child_window(title="Address and search bar", control_type="Edit")
  omnibox.set_edit_text('anything').type_keys('{ENTER}')
  print(driver.current_url)
except Exception as error:
  print(error)
finally:
  driver.quit()
