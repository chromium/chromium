# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from absl import app

from selenium.webdriver.common.by import By
from test_util import create_chrome_webdriver


def main(argv):
  driver = create_chrome_webdriver()

  try:
    driver.get('chrome://policy')
    driver.find_element(By.ID, 'reload-policies').click

  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
