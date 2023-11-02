# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import app
from selenium import webdriver

from test_util import create_chrome_webdriver


def main(argv):
  testSite = "http://www.dummysoftware.com/popupdummy_testpage.html"
  options = webdriver.ChromeOptions()
  options.add_experimental_option('excludeSwitches', ['disable-popup-blocking'])
  driver = create_chrome_webdriver(chrome_options=options)
  driver.implicitly_wait(5)
  driver.get(testSite)
  handles = driver.window_handles
  print(len(handles))
  driver.quit()


if __name__ == '__main__':
  app.run(main)
