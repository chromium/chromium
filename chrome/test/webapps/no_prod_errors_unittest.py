#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from io import StringIO
import os
import sys
from typing import Dict
import unittest
import tempfile

from generate_framework_tests_and_coverage import main


class NoOutput(unittest.TestCase):
    def test_coverage(self):
        capturedOutput = StringIO()
        sys.stdout = capturedOutput
        main(['--suppress-coverage'])
        # The framework uses stdout to inform the developer of tests that
        # need to be added or removed. Since there should be no tests
        # changes required, nothing should be printed to stdout.
        self.assertFalse(capturedOutput.read())
        sys.stdout = sys.__stdout__


if __name__ == '__main__':
    unittest.main()
