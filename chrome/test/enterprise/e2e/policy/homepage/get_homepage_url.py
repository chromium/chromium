# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pywinauto.application import Application

from test_util import create_chrome_webdriver

driver = create_chrome_webdriver()

try:
  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')

  # Use shortcut Alt+HOME to go to the home page
  app.top_window().type_keys("%{HOME}")

  print('homepage:%s' % driver.current_url)
finally:
  driver.quit()
