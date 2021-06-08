#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import csv
from typing import List
import unittest

from file_reading import get_tests_in_browsertest
from file_reading import read_actions_file
from file_reading import read_unprocessed_coverage_tests_file
from models import ActionsByName
from models import CoverageTest
from models import TestPlatform

TEST_DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "test_data")


class TestAnalysisTest(unittest.TestCase):
    def test_action_file_reading(self):
        actions_filename = os.path.join(TEST_DATA_DIR, "test_actions.csv")
        with open(actions_filename) as f:
            actions_csv = csv.reader(f, delimiter=',')
            (actions, action_base_name_to_default_param
             ) = read_actions_file(actions_csv)
            self.assertEqual(len(actions), 10)
            self.assertEqual(len(action_base_name_to_default_param), 4)

            # Check parameterized action state.
            self.assertTrue('changes_Mode1' in actions)
            self.assertTrue('changes_Mode2' in actions)

            self.assertTrue('checks' in actions)
            checks_output_actions = actions['checks'].output_actions
            self.assertEqual(len(checks_output_actions), 2)
            self.assertCountEqual(
                checks_output_actions,
                [actions['check_a_Mode1'], actions['check_b_Mode1']])

    def test_coverage_file_reading(self):
        actions_filename = os.path.join(TEST_DATA_DIR, "test_actions.csv")
        actions: ActionsByName = {}
        action_base_name_to_default_param = {}
        with open(actions_filename) as f:
            actions_csv = csv.reader(f, delimiter=',')
            (actions, action_base_name_to_default_param
             ) = read_actions_file(actions_csv)

        coverage_filename = os.path.join(TEST_DATA_DIR,
                                         "test_unprocessed_coverage.csv")
        coverage_tests: List[CoverageTest] = []
        with open(coverage_filename) as f:
            coverage_csv = csv.reader(f, delimiter=',')
            coverage_tests = read_unprocessed_coverage_tests_file(
                coverage_csv, actions, action_base_name_to_default_param)

        self.assertEqual(len(coverage_tests), 4)

    def test_browsertest_detection(self):
        browsertest_filename = os.path.join(TEST_DATA_DIR, "tests_default.cc")
        with open(browsertest_filename) as browsertest_file:
            tests_and_platforms = get_tests_in_browsertest(
                browsertest_file.read())
            self.assertListEqual(list(tests_and_platforms.keys()),
                                 ["ChngAMode1_ChckAMode1_ChckBMode1"])
            tests_and_platforms = tests_and_platforms[
                "ChngAMode1_ChckAMode1_ChckBMode1"]
            self.assertEqual(
                {TestPlatform.LINUX, TestPlatform.CHROME_OS, TestPlatform.MAC},
                tests_and_platforms)


if __name__ == '__main__':
    unittest.main()
