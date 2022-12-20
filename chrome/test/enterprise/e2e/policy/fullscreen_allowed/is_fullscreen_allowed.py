# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
from absl import app
from pywinauto.application import Application

from test_util import create_chrome_webdriver


def main(argv):
  driver = create_chrome_webdriver()
  try:
    application = Application(backend="uia")
    application.connect(title_re='.*Chrome|.*Chromium')
    w = application.top_window()

    for desc in w.descendants():
      print("item: %s" % desc)

    print("Closing info bar if exists.")
    if w.child_window(best_match="Infobar Container").exists():
      w.child_window(best_match="Infobar Container").child_window(
          best_match="Close").click_input()

    print("press F11 to enter full screen mode.")
    w.type_keys('{F11}')

    time.sleep(5)
    window_rect = w.rectangle()
    window_width = window_rect.width()
    window_height = window_rect.height()
    content_width = driver.execute_script("return window.innerWidth")
    content_height = driver.execute_script("return window.innerHeight")

    # The content area should be the same size as the full window.
    print("window_rect: %s" % window_rect)
    print("window_width: %s" % window_width)
    print("window_height: %s" % window_height)
    print("content_width: %s" % content_width)
    print("content_height: %s" % content_height)

    fs = window_width == content_width and window_height == content_height
    print("FullscreenAllowed: %s" % fs)
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
