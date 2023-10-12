#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import csv
from io import StringIO
import os
import tempfile
from typing import List, Set
import shutil
import sys
import unittest

from file_reading import get_and_maybe_delete_tests_in_browsertest
from file_reading import read_actions_file, read_enums_file
from file_reading import read_platform_supported_actions
from file_reading import read_unprocessed_coverage_tests_file
from models import Action
from models import EnumsByType
from models import ActionsByName
from models import ActionType
from models import CoverageTest
from models import CoverageTestsByPlatform
from models import CoverageTestsByPlatformSet
from models import TestIdsTestNamesByPlatformSet
from models import TestIdTestNameTuple
from models import TestPartitionDescription
from models import TestPlatform
from test_analysis import compare_and_print_tests_to_remove_and_add
from test_analysis import expand_parameterized_tests
from test_analysis import partition_framework_tests_per_platform_combination

TEST_DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "test_data")


def CreateDummyAction(id: str):
    return Action(id, id, id, id, ActionType.STATE_CHANGE, TestPlatform,
                  TestPlatform)


def CreateCoverageTest(id: str, platforms: Set[TestPlatform]):
    return CoverageTest([CreateDummyAction(id)], platforms)


def CreateNewDummyTestByPlatformSet(
        platforms: Set[TestPlatform]) -> CoverageTestsByPlatformSet:
    new_test_by_platform: CoverageTestsByPlatform = {}
    for platform in platforms:
        # Add the simple dummy test of an action "a" to all platforms.
        new_test_by_platform[platform] = ([
            CreateCoverageTest("a", {platform})
        ])
    return partition_framework_tests_per_platform_combination(
        new_test_by_platform)


def GetExistingTestIdsTestNamesByPlatformSet(
        filename: str, required_tests: Set[TestIdTestNameTuple],
        delete_in_place: bool) -> TestIdsTestNamesByPlatformSet:
    # Read in existing tests from a file.
    platforms = frozenset(
        TestPlatform.get_platforms_from_browsertest_filename(filename))
    existing_tests_in_file = get_and_maybe_delete_tests_in_browsertest(
        filename,
        required_tests=required_tests,
        delete_in_place=delete_in_place)
    existing_tests: TestIdsTestNamesByPlatformSet = defaultdict(lambda: set())
    for (test_id, test_name) in existing_tests_in_file.keys():
        existing_tests[platforms].add(TestIdTestNameTuple(test_id, test_name))
    return existing_tests


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

    def test_compare_and_print_tests_to_remove_and_add_add_to_existing_file(
            self):
        with tempfile.TemporaryDirectory(dir=TEST_DATA_DIR) as tmpdirname:
            original_file = os.path.join(TEST_DATA_DIR,
                                         "tests_for_deletion_addition.cc")
            test_file = os.path.join(tmpdirname,
                                     "tests_for_deletion_addition.cc")
            shutil.copyfile(original_file, test_file)
            test_platforms: Set[TestPlatform] = {
                TestPlatform.WINDOWS,
                TestPlatform.MAC,
                TestPlatform.LINUX,
                TestPlatform.CHROME_OS,
            }
            new_test_required_by_platform_set: CoverageTestsByPlatformSet = (
                CreateNewDummyTestByPlatformSet(test_platforms))
            existing_tests: TestIdsTestNamesByPlatformSet = (
                GetExistingTestIdsTestNamesByPlatformSet(
                    filename=test_file,
                    required_tests={
                        TestIdTestNameTuple(
                            "state_change_a_Chicken_check_a_Chicken_check_b_Chicken_Green",
                            "3Chicken_1Chicken_2ChickenGreen")
                    },
                    delete_in_place=False))
            default_partition = TestPartitionDescription(
                action_name_prefixes=set(),
                browsertest_dir=tmpdirname,
                test_file_prefix="tests_for_deletion_addition",
                test_fixture="TestName")
            compare_and_print_tests_to_remove_and_add(
                existing_tests,
                new_test_required_by_platform_set,
                test_partitions=[],
                default_partition=default_partition,
                add_to_file=True)
            expected_file = os.path.join(TEST_DATA_DIR, "expected_test_txt",
                                         "tests_change_for_adding_test.cc")
            with open(expected_file, "r") as f, open(test_file, "r") as f2:
                self.assertEqual(f.read(), f2.read())

    def test_compare_and_print_tests_with_same_name_diff_check_actions_only(
            self):
        actions_filename = os.path.join(TEST_DATA_DIR, "test_actions.md")
        supported_actions_filename = os.path.join(
            TEST_DATA_DIR, "framework_supported_actions.csv")
        enums_filename = os.path.join(TEST_DATA_DIR, "test_enums.md")

        actions: ActionsByName = {}
        action_base_name_to_default_param = {}
        with open(actions_filename) as f, \
                open(supported_actions_filename, "r", encoding="utf-8") \
                    as supported_actions, \
                open(enums_filename, "r", encoding="utf-8") as enums:
            supported_actions = read_platform_supported_actions(
                csv.reader(supported_actions, delimiter=','))
            actions_tsv = f.readlines()
            enums = read_enums_file(enums.readlines())
            (actions, action_base_name_to_default_param) = read_actions_file(
                actions_tsv, enums, supported_actions)

        coverage_filename = os.path.join(TEST_DATA_DIR,
                                         "test_addition_coverage.md")
        generated_coverage_tests: List[CoverageTest] = []
        with open(coverage_filename) as f:
            coverage_tsv = f.readlines()
            generated_coverage_tests = read_unprocessed_coverage_tests_file(
                coverage_tsv, actions, enums,
                action_base_name_to_default_param)

        test_platforms: Set[TestPlatform] = {
            TestPlatform.WINDOWS,
            TestPlatform.MAC,
            TestPlatform.LINUX,
            TestPlatform.CHROME_OS,
        }
        new_tests_by_platform: CoverageTestsByPlatform = {}
        for platform in test_platforms:
            new_tests_by_platform[platform] = [generated_coverage_tests[0]]
        new_coverage_tests_by_platform_set = (
            partition_framework_tests_per_platform_combination(
                new_tests_by_platform))

        with tempfile.TemporaryDirectory(dir=TEST_DATA_DIR) as tmpdirname:
            original_file = os.path.join(
                TEST_DATA_DIR,
                "tests_change_for_replacing_test_same_test_name.cc")
            test_file = os.path.join(
                tmpdirname,
                "tests_change_for_replacing_test_same_test_name.cc")
            shutil.copyfile(original_file, test_file)
            existing_tests: TestIdsTestNamesByPlatformSet = (
                GetExistingTestIdsTestNamesByPlatformSet(
                    filename=test_file,
                    required_tests={
                        TestIdTestNameTuple(
                            "state_change_a_Chicken_state_change_a_Dog_check_a_Dog",
                            "StateChangeAChicken_StateChangeADog")
                    },
                    delete_in_place=True))

            default_partition = TestPartitionDescription(
                action_name_prefixes=set(),
                browsertest_dir=tmpdirname,
                test_file_prefix=
                "tests_change_for_replacing_test_same_test_name",
                test_fixture="TestName")
            compare_and_print_tests_to_remove_and_add(
                existing_tests,
                new_coverage_tests_by_platform_set,
                test_partitions=[],
                default_partition=default_partition,
                add_to_file=True)

            expected_file = os.path.join(
                TEST_DATA_DIR, "expected_test_txt",
                "tests_change_for_replacing_test_same_test_name.cc")
            with open(expected_file, "r") as f, open(test_file, "r") as f2:
                self.assertEqual(f.read(), f2.read())

    def test_compare_and_print_tests_to_remove_and_add_delete_and_add_to_file(
            self):
        with tempfile.TemporaryDirectory(dir=TEST_DATA_DIR) as tmpdirname:
            original_file = os.path.join(TEST_DATA_DIR,
                                         "tests_for_deletion_addition.cc")
            test_file = os.path.join(tmpdirname,
                                     "tests_for_deletion_addition.cc")
            shutil.copyfile(original_file, test_file)

            test_platforms: Set[TestPlatform] = {
                TestPlatform.WINDOWS,
                TestPlatform.MAC,
                TestPlatform.LINUX,
                TestPlatform.CHROME_OS,
            }
            new_test_required_by_platform_set: CoverageTestsByPlatformSet = (
                CreateNewDummyTestByPlatformSet(test_platforms))
            existing_tests: TestIdsByPlatformSet = (
                GetExistingTestIdsTestNamesByPlatformSet(filename=test_file,
                                                         required_tests={},
                                                         delete_in_place=True))

            default_partition = TestPartitionDescription(
                action_name_prefixes=set(),
                browsertest_dir=tmpdirname,
                test_file_prefix="tests_for_deletion_addition",
                test_fixture="TestName")
            compare_and_print_tests_to_remove_and_add(
                existing_tests,
                new_test_required_by_platform_set,
                test_partitions=[],
                default_partition=default_partition,
                add_to_file=True)

            expected_file = os.path.join(
                TEST_DATA_DIR, "expected_test_txt",
                "tests_change_for_deleting_adding_test.cc")
            with open(expected_file, "r") as f, open(test_file, "r") as f2:
                self.assertEqual(f.read(), f2.read())

    def test_compare_and_print_tests_to_remove_and_add_add_to_new_file(self):
        with tempfile.TemporaryDirectory(dir=TEST_DATA_DIR) as tmpdirname:
            original_file = os.path.join(TEST_DATA_DIR,
                                         "tests_for_deletion_addition.cc")
            test_file = os.path.join(tmpdirname,
                                     "tests_for_deletion_addition.cc")
            shutil.copyfile(original_file, test_file)

            test_platforms: Set[TestPlatform] = {
                TestPlatform.WINDOWS, TestPlatform.MAC
            }
            new_test_required_by_platform_set: CoverageTestsByPlatformSet = (
                CreateNewDummyTestByPlatformSet(test_platforms))
            existing_tests: TestIdsByPlatformSet = (
                GetExistingTestIdsTestNamesByPlatformSet(test_file, {}, True))

            default_partition = TestPartitionDescription(
                action_name_prefixes=set(),
                browsertest_dir=tmpdirname,
                test_file_prefix="tests_for_deletion_addition",
                test_fixture="WebAppIntegration")

            captured_output = StringIO()
            sys.stdout = captured_output
            compare_and_print_tests_to_remove_and_add(
                existing_tests,
                new_test_required_by_platform_set,
                test_partitions=[],
                default_partition=default_partition,
                add_to_file=True)
            console_output_str = captured_output.getvalue()
            sys.stdout = sys.__stdout__

            expected_file = os.path.join(
                TEST_DATA_DIR, "expected_test_txt",
                "tests_change_for_deletion_addition_mac_win.txt")
            test_output_file = os.path.join(
                    tmpdirname, "tests_for_deletion_addition_mac_win.cc")
            with open(expected_file, "r") as f:
                self.assertEqual(f.read() % test_output_file,
                                 console_output_str)


if __name__ == '__main__':
    unittest.main()
