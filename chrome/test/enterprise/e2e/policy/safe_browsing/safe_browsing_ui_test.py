# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import time
from absl import app
from selenium import webdriver
from pywinauto.application import Application

from test_util import create_chrome_webdriver

UnsafePageLink = "http://testsafebrowsing.appspot.com/s/malware.html"
UnsafePageLinkTabText = "Security error"

UnsafeDownloadLink = "http://testsafebrowsing.appspot.com/s/badrep.exe"
UnsafeDownloadTextRe = ".* is dangerous,\s*so\s*Chrom.* has blocked it"


def visit(window, url):
  """Visit a specific URL through pywinauto.Application.

  SafeBrowsing intercepts HTTP requests & hangs WebDriver.get(), which prevents
  us from getting the page source. Using pywinauto to visit the pages instead.
  """
  window.Edit.set_edit_text(url).type_keys("%{ENTER}")
  time.sleep(10)


def main(argv):
  exclude_switches = ["disable-background-networking"]
  chrome_options = webdriver.ChromeOptions()
  chrome_options.add_experimental_option("excludeSwitches", exclude_switches)

  driver = create_chrome_webdriver(chrome_options=chrome_options)

  app = Application(backend="uia")
  app.connect(title_re='.*Chrome|.*Chromium')
  window = app.top_window()

  # Wait for Chrome to download SafeBrowsing lists in the background.
  # There's no trigger to force this operation or synchronize on it, but quick
  # experiments have shown 3-4 minutes in most cases, so 5 should be plenty.
  time.sleep(60 * 5)

  print("Visiting unsafe page: %s" % UnsafePageLink)
  visit(window, UnsafePageLink)

  unsafe_page = False
  for desc in app.top_window().descendants():
    if desc.window_text():
      print("unsafe_page.item: %s" % desc.window_text())
      if UnsafePageLinkTabText in desc.window_text():
        unsafe_page = True
        break

  print("Downloading unsafe file: %s" % UnsafeDownloadLink)
  visit(window, UnsafeDownloadLink)

  unsafe_download = False
  for desc in app.top_window().descendants():
    if desc.window_text():
      print("unsafe_download.item: %s" % desc.window_text())
      if re.search(UnsafeDownloadTextRe, desc.window_text()):
        unsafe_download = True
        break

  print("RESULTS.unsafe_page: %s" % unsafe_page)
  print("RESULTS.unsafe_download: %s" % unsafe_download)

  driver.quit()


if __name__ == '__main__':
  app.run(main)
