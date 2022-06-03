#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run tools/ unittests."""

import sys
import unittest

if __name__ == '__main__':
  suite = unittest.TestLoader().discover('tools', pattern = "*_unittest.py")
  sys.exit(0 if unittest.TextTestRunner().run(suite).wasSuccessful() else 1)
