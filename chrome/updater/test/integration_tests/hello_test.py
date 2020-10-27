# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sample test case to run on chromium infra."""

import unittest

class HelloTest(unittest.TestCase):

  def test_hello(self):
    pass

if __name__ == '__main__':
  unittest.main()

