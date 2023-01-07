#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import os
from typing import List, Set
import unittest

from file_reading import read_actions_file, read_enums_file
from file_reading import read_platform_supported_actions
from file_reading import read_unprocessed_coverage_tests_file
from models import Action
from models import EnumsByType
from models import ActionsByName
from models import ActionType
from models import CoverageTest
from models import CoverageTestsByPlatform
from models import TestPlatform
from test_analysis import expand_parameterized_tests
from test_analysis import partition_framework_tests_per_platform_combination

TEST_DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "test_data")


def CreateDummyAction(id: str):
    return Action(id, id, id, id, ActionType.STATE_CHANGE, TestPlatform,
                  TestPlatform)


def CreateCoverageTest(id: str, platforms: Set[TestPlatform]):
    return CoverageTest([CreateDummyAction(id)], platforms)


class TestAnalysisTest(unittest.TestCase):
    def test_partition_framework_tests_per_platform_combination(self):
        tests_by_platform: CoverageTestsByPlatform = {}
        windows_tests = []
        windows_tests.append(CreateCoverageTest("a", {TestPlatform.WINDOWS}))
        windows_tests.append(CreateCoverageTest("b", {TestPlatform.WINDOWS}))
        windows_tests.append(CreateCoverageTest("c", {TestPlatform.WINDOWS}))
        tests_by_platform[TestPlatform.WINDOWS] = windows_tests
        mac_tests = []
        mac_tests.append(CreateCoverageTest("a", {TestPlatform.MAC}))
        mac_tests.append(CreateCoverageTest("c", {TestPlatform.MAC}))
        tests_by_platform[TestPlatform.MAC] = mac_tests
        linux_tests = []
        linux_tests.append(CreateCoverageTest("a", {TestPlatform.LINUX}))
        tests_by_platform[TestPlatform.LINUX] = linux_tests

        partitions = partition_framework_tests_per_platform_combination(
            tests_by_platform)
        self.assertEqual(len(partitions), 3)

        self.assertTrue(frozenset({TestPlatform.WINDOWS}) in partitions)
        windows_tests = partitions[frozenset({TestPlatform.WINDOWS})]
        self.assertEqual(len(windows_tests), 1)
        self.assertEqual(windows_tests[0].id, "b")

        self.assertTrue(
            frozenset({TestPlatform.MAC, TestPlatform.WINDOWS}) in partitions)
        mac_win_tests = partitions[frozenset(
            {TestPlatform.MAC, TestPlatform.WINDOWS})]
        self.assertEqual(len(mac_win_tests), 1)
        self.assertEqual(mac_win_tests[0].id, "c")

        mac_win_linux_key = frozenset(
            {TestPlatform.MAC, TestPlatform.WINDOWS, TestPlatform.LINUX})
        self.assertTrue(mac_win_linux_key in partitions)
        mac_win_linux_tests = partitions[mac_win_linux_key]
        self.assertEqual(len(mac_win_linux_tests), 1)
        self.assertEqual(mac_win_linux_tests[0].id, "a")

    def test_processed_coverage(self):
        actions_filename = os.path.join(TEST_DATA_DIR, "test_actions.md")
        supported_actions_filename = os.path.join(
            TEST_DATA_DIR, "framework_supported_actions.csv")
        enums_filename = os.path.join(TEST_DATA_DIR, "test_enums.md")

        actions: ActionsByName = {}
        action_base_name_to_default_param = {}
        enums: EnumsByType = {}
        with open(actions_filename, "r", encoding="utf-8") as f, \
                open(supported_actions_filename, "r", encoding="utf-8") \
                    as supported_actions_file, \
                open(enums_filename, "r", encoding="utf-8") as enums:
            supported_actions = read_platform_supported_actions(
                csv.reader(supported_actions_file, delimiter=','))
            enums = read_enums_file(enums.readlines())
            (actions, action_base_name_to_default_param) = read_actions_file(
                f.readlines(), enums, supported_actions)

        coverage_filename = os.path.join(TEST_DATA_DIR,
                                         "test_unprocessed_coverage.md")
        coverage_tests: List[CoverageTest] = []
        with open(coverage_filename, "r", encoding="utf-8") as f:
            coverage_tests = read_unprocessed_coverage_tests_file(
                f.readlines(), actions, enums,
                action_base_name_to_default_param)
        coverage_tests = expand_parameterized_tests(coverage_tests)

        # Compare with expected
        expected_processed_tests = []
        processed_filename = os.path.join(TEST_DATA_DIR,
                                          "expected_processed_coverage.md")
        with open(processed_filename, "r", encoding="utf-8") as f:
            expected_processed_tests = read_unprocessed_coverage_tests_file(
                f.readlines(), actions, enums,
                action_base_name_to_default_param)

        # Hack for easy comparison and printing: transform coverage tests into
        # a Tuple[List[str], Set[TestPlatform]].
        self.assertCountEqual([([action.name
                                 for action in test.actions], test.platforms)
                               for test in coverage_tests],
                              [([action.name
                                 for action in test.actions], test.platforms)
                               for test in expected_processed_tests])


if __name__ == '__main__':
    unittest.main()
