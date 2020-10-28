# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sample test case to run on chromium infra."""

import logging
import typ

class HelloTest(typ.TestCase):

  def test_hello(self):
    logging.info('Hello World!')
    pass

  def test_build_dir(self):
    logging.info('Build dir = %s', self.context.build_dir)
    self.assertTrue(self.context.build_dir is not None)


if __name__ == '__main__':
  unittest.main()
