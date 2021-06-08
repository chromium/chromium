#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from models import TestPlatform


class ModelsTest(unittest.TestCase):
    def test_platforms_from_chars(self):
        self.assertCountEqual({TestPlatform.LINUX},
                              TestPlatform.get_platforms_from_chars("L"))
        self.assertCountEqual({TestPlatform.MAC},
                              TestPlatform.get_platforms_from_chars("M"))
        self.assertCountEqual({TestPlatform.WINDOWS},
                              TestPlatform.get_platforms_from_chars("W"))
        self.assertCountEqual({TestPlatform.CHROME_OS},
                              TestPlatform.get_platforms_from_chars("C"))
        self.assertCountEqual({TestPlatform.WINDOWS, TestPlatform.CHROME_OS},
                              TestPlatform.get_platforms_from_chars("WC"))

    def test_platforms_from_browsertest_filename(self):
        self.assertCountEqual(
            {TestPlatform.LINUX},
            TestPlatform.get_platforms_from_browsertest_filename(
                "browsertest_name_linux.cc"))
        self.assertCountEqual(
            {TestPlatform.MAC},
            TestPlatform.get_platforms_from_browsertest_filename(
                "browsertest_name_mac.cc"))
        self.assertCountEqual(
            {TestPlatform.WINDOWS},
            TestPlatform.get_platforms_from_browsertest_filename(
                "browsertest_name_win.cc"))
        self.assertCountEqual(
            {TestPlatform.CHROME_OS},
            TestPlatform.get_platforms_from_browsertest_filename(
                "browsertest_name_cros.cc"))
        self.assertCountEqual(
            {TestPlatform.WINDOWS, TestPlatform.CHROME_OS},
            TestPlatform.get_platforms_from_browsertest_filename(
                "browsertest_name_win_cros.cc"))
        # No suffixes should apply to all platforms
        self.assertCountEqual(
            TestPlatform,
            TestPlatform.get_platforms_from_browsertest_filename(
                "browsertest_name.cc"))


if __name__ == '__main__':
    unittest.main()
