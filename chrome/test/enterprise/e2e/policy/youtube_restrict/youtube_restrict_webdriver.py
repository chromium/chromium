# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from absl import app, flags
from selenium.common.exceptions import NoSuchElementException
from selenium.webdriver.common.by import By

from test_util import create_chrome_webdriver

FLAGS = flags.FLAGS

flags.DEFINE_string('url', None, 'The url to open in Chrome.')
flags.mark_flag_as_required('url')

flags.DEFINE_integer('wait', 0, 'How many seconds to wait to load the page')


def main(argv):
  driver = create_chrome_webdriver()

  try:
    driver.get(FLAGS.url)
    if FLAGS.wait > 0:
      time.sleep(FLAGS.wait)

    print(
        driver.find_element(By.XPATH,
                            '//*[@id="header"]/ytd-text-header-renderer').text)
  except NoSuchElementException:
    print("Restricted text not found")
  finally:
    driver.quit()


if __name__ == '__main__':
  app.run(main)
