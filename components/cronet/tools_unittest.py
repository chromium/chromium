#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run tools/ unittests."""

import sys
import unittest

if __name__ == '__main__':
  # jar/java/javac aren't typically installed on Windows so these tests don't
  # work and give many verbose and cryptic failure messages.
  if sys.platform == 'win32':
    sys.exit(0)
  suite = unittest.TestLoader().discover('tools', pattern = "*_unittest.py")
  sys.exit(0 if unittest.TextTestRunner().run(suite).wasSuccessful() else 1)
