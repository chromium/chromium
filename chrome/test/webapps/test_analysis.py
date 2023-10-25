#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test analysis functions for the testing framework.
"""

from collections import defaultdict
import datetime
import logging
import re
import os
from typing import List, Set

from models import Action
from models import CoverageTest
from models import CoverageTestsByPlatform
from models import ActionType
from models import CoverageTestsByPlatformSet
from models import TestId
from models import TestIdsTestNamesByPlatformSet
from models import TestIdTestNameTuple
from models import TestPartitionDescription
from models import TestPlatform


def filter_tests_for_partition(tests: List[CoverageTest],
                               partition: TestPartitionDescription
                               ) -> List[CoverageTest]:
    def DoesTestHaveActionWithPrefixes(test: CoverageTest):
        """Returns if the given tests has any actions with the given prefixes"""
        nonlocal partition
        for action in test.actions:
            for prefix in partition.action_name_prefixes:
                if action.name.startswith(prefix):
                    return True

    return list(filter(DoesTestHaveActionWithPrefixes, tests))


def compare_and_print_tests_to_remove_and_add(
        existing_tests: TestIdsTestNamesByPlatformSet,
        required_tests: CoverageTestsByPlatformSet,
        test_partitions: List[TestPartitionDescription],
        default_partition: TestPartitionDescription, add_to_file: bool):
    """
    Given the existing tests on disk and the required tests, print out the
    changes that need to happen to make them match. This also takes into account
    test partitioning, so tests are asked to be written to the appropriate test
    partition file.
    Note: This does NOT support moving tests between partition files. If a test
    was found in any partition file, then it is ignored.
    """
    def print_tests(filename: str, tests: List[CoverageTest],
                    partition: TestPartitionDescription, add_to_file: bool):
        new_test_str: str = ""
        for test in tests:
            new_test_str += ("\n" + test.generate_browsertest(partition) +
                             "\n")
        if add_to_file:
            if os.path.exists(filename):
                with open(filename, "r") as f:
                    test_file = f.read()
                # Find the last test in the test file
                if re.search(r"IN_PROC_BROWSER_TEST_F(.|\n)*?}\n", test_file):
                    res = re.finditer(r"IN_PROC_BROWSER_TEST_F(.|\n)*?}\n",
                                      test_file)
                    last_test_end_index = list(res)[-1].end()
                # Find the first closing parenthesis (end of namespace) if
                # there is no test in the file
                elif "}" in test_file:
                    last_test_end_index = test_file.find("}") - 1
                else:
                    last_test_end_index = len(test_file)
                new_content = (test_file[:last_test_end_index] + new_test_str +
                               test_file[last_test_end_index:])
                with open(filename, "w") as f:
                    f.write(new_content)
            else:
                print(f"\n\nCreate a new test file: {filename}\n"
                      "Remember to add the new test file to the BUILD file.\n"
                      "Add the following tests to the new test file:\n"
                      f"{new_test_str}")
        else:
            print(f"\n\nAdd the following tests to {filename}:\n"
                  f"{new_test_str}")

    test_ids_to_keep: TestIdsByPlatformSet = defaultdict(lambda: set())
    for platforms, tests in required_tests.items():
        tests_to_add: List[CoverageTest] = []
        for test in tests:
            if platforms in existing_tests:
                existing_test_set = set(
                    [test_id for (test_id, _) in existing_tests[platforms]])
                if test.id not in existing_test_set:
                    tests_to_add.append(test)
                else:
                    test_ids_to_keep[platforms].add(test.id)
            else:
                tests_to_add.append(test)
        tests_added_to_partition: Set[TestId] = set()
        for partition in test_partitions:
            tests_to_add_partition = filter_tests_for_partition(
                tests_to_add, partition)
            if not tests_to_add_partition:
                continue
            # Record all tests to ensure we don't have duplicates in different
            # files, and to output remaining tests to the default partition.
            for test in tests_to_add_partition:
                if test.id in tests_added_to_partition:
                    raise ValueError(
                        "Cannot have a test written to multiple test files.")
                tests_added_to_partition.add(test.id)
            filename = partition.generate_browsertest_filepath(platforms)
            print_tests(filename, tests_to_add_partition, partition,
                        add_to_file)

        # All remaining tests go into the default partition
        default_tests: List[CoverageTest] = [
            test for test in tests_to_add
            if test.id not in tests_added_to_partition
        ]
        if not default_tests:
            continue
        filename = default_partition.generate_browsertest_filepath(platforms)
        print_tests(filename, default_tests, default_partition, add_to_file)
    # Print out all tests to remove. To keep the algorithm simple the partition
    # is not kept track of.
    for platforms, test_ids_names in existing_tests.items():
        tests_to_remove = []
        prompt_str = ""
        nice_platform_str = ", ".join(
            [f"{platform}" for platform in platforms])
        if platforms not in test_ids_to_keep:
            prompt_str = (f"\n\nRemove ALL tests from the file for the "
                          f"platforms [{nice_platform_str}]:\n")
            tests_to_remove = [test_name for (_, test_name) in test_ids_names]
        else:
            prompt_str = (f"\n\nRemove these tests from the file for the "
                          f"platforms [{nice_platform_str}]:\n")
            tests_to_remove = [
                test_name for (test_id, test_name) in test_ids_names
                if test_id not in test_ids_to_keep[platforms]
            ]

        if not tests_to_remove:
            continue
        print(f"{prompt_str}{', '.join(tests_to_remove)}")


def expand_parameterized_tests(coverage_tests: List[CoverageTest]
                               ) -> List[CoverageTest]:
    """
    Takes a list of coverage tests that contain parameterized actions, and
    expands all of the tests with those actions to result in a list of tests
    without parameterized actions.
    """

    def get_all_parameterized_tests(test_actions: List[Action]
                                    ) -> List[List[Action]]:
        """
        Takes a list of actions with possible parameterized actions, and outputs
        a list of resulting tests with all parameterized actions expanded.
        """
        if not test_actions:
            return [[]]
        for i, action in enumerate(test_actions):
            if action.type is not ActionType.PARAMETERIZED:
                continue
            actions_before_parameterized = test_actions[:i]
            actions_after_parameterized = test_actions[i + 1:]
            resulting_tests = []
            for output_action in action.output_actions:
                remaining_expanded_tests = get_all_parameterized_tests(
                    actions_after_parameterized)
                for remaining_test in remaining_expanded_tests:
                    test = (actions_before_parameterized + [output_action] +
                            remaining_test)
                    resulting_tests.append(test)
            return resulting_tests
        # No parameterized actions were found, so just return the test actions.
        return [test_actions]

    result_tests = []
    for test in coverage_tests:
        expanded_tests = get_all_parameterized_tests(test.actions)
        logging.info(f"Generated {len(expanded_tests)} test/s from {test.id}")
        for resulting_test in get_all_parameterized_tests(test.actions):
            result_tests.append(CoverageTest(resulting_test, test.platforms))
    return result_tests


def filter_coverage_tests_for_platform(tests: List[CoverageTest],
                                       platform: TestPlatform
                                       ) -> List[CoverageTest]:
    def IsSupportedOnPlatform(test: CoverageTest):
        return platform in test.platforms

    return list(filter(IsSupportedOnPlatform, tests))


def partition_framework_tests_per_platform_combination(
        generated_tests_per_platform: CoverageTestsByPlatform
) -> CoverageTestsByPlatformSet:
    test_id_to_platforms = defaultdict(lambda: set())
    test_id_to_test = {}
    platform_set_to_tests = defaultdict(lambda: list())
    for platform, tests in generated_tests_per_platform.items():
        for test in tests:
            test_id_to_platforms[test.id].add(platform)
            if test.id not in test_id_to_test:
                test_id_to_test[test.id] = CoverageTest(test.actions, set())
            test_id_to_test[test.id].platforms.add(platform)
    for test_id, platforms in test_id_to_platforms.items():
        platforms = frozenset(platforms)
        platform_set_to_tests[platforms].append(test_id_to_test[test_id])
    return platform_set_to_tests
