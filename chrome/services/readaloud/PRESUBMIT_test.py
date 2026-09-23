#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
# chromium/src is 3 levels up from chrome/services/readaloud
sys.path.insert(0, os.path.join(file_dir_path, '..', '..', '..'))
from PRESUBMIT_test_mocks import MockAffectedFile, MockInputApi, MockOutputApi


class ServicesPresubmitWrapperTest(unittest.TestCase):

    def testDelegatesValidActualExpected(self):
        lines = [
            '  EXPECT_EQ(result, 0);',
            '  EXPECT_EQ(segment->sample_rate(), 44100);',
            '  EXPECT_EQ(bounds.start_time, base::Milliseconds(100));',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/services/readaloud/foo_unittest.cc', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(0, len(results))

    def testDelegatesInvalidExpectedActual(self):
        lines = [
            '  EXPECT_EQ(0, result);',
            '  EXPECT_EQ(kChannels, segment->channel_count());',
        ]
        mock_input = MockInputApi()
        mock_input.files = [
            MockAffectedFile('chrome/services/readaloud/foo_unittest.cc', lines)
        ]
        mock_output = MockOutputApi()
        results = PRESUBMIT.CheckTestAssertionOrder(mock_input, mock_output)
        self.assertEqual(1, len(results))
        self.assertIn('EXPECT_EQ(0, result)', results[0].message)
        self.assertIn(
            'EXPECT_EQ(kChannels, segment->channel_count())',
            results[0].message,
        )


if __name__ == '__main__':
    unittest.main()
