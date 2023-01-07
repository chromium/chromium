#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
from file_reading import read_actions_file, read_enums_file, read_platform_supported_actions, read_unprocessed_coverage_tests_file
from test_analysis import expand_parameterized_tests, filter_coverage_tests_for_platform, partition_framework_tests_per_platform_combination
from graph_analysis import build_action_node_graph, generate_framework_tests, trim_graph_to_platform_actions
import os
import unittest

from models import ActionNode, CoverageTestsByPlatform, CoverageTestsByPlatformSet, TestPartitionDescription
from models import TestPlatform

TEST_DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "test_data")


class GraphAnalysisUnittest(unittest.TestCase):
    def test_test_generation(self):
        self.maxDiff = None
        actions_filename = os.path.join(TEST_DATA_DIR, "test_actions.md")
        enums_filename = os.path.join(TEST_DATA_DIR, "test_enums.md")
        supported_actions_filename = os.path.join(
            TEST_DATA_DIR, "framework_supported_actions.csv")

        coverage_filename = os.path.join(TEST_DATA_DIR,
                                         "test_unprocessed_coverage.md")

        test_partition = TestPartitionDescription(
            action_name_prefixes=set(),
            browsertest_dir=os.path.join(TEST_DATA_DIR, "expected_test_txt"),
            test_file_prefix="tests_default",
            test_fixture="TestName")

        with open(actions_filename, "r", encoding="utf-8") as actions_file, \
                open(supported_actions_filename, "r", encoding="utf-8") \
                    as supported_actions_file, \
                open (enums_filename, "r", encoding="utf-8") as enums, \
                open(coverage_filename, "r", encoding="utf-8") \
                    as coverage_file:
            enums = read_enums_file(enums.readlines())
            platform_supported_actions = read_platform_supported_actions(
                csv.reader(supported_actions_file, delimiter=','))
            (actions, action_base_name_to_default_param) = read_actions_file(
                actions_file.readlines(), enums, platform_supported_actions)

            required_coverage_tests = read_unprocessed_coverage_tests_file(
                coverage_file.readlines(), actions, enums,
                action_base_name_to_default_param)

            required_coverage_tests = expand_parameterized_tests(
                required_coverage_tests)

            required_coverage_by_platform: CoverageTestsByPlatform = {}
            generated_tests_by_platform: CoverageTestsByPlatform = {}
            for platform in TestPlatform:
                platform_tests = filter_coverage_tests_for_platform(
                    required_coverage_tests.copy(), platform)
                required_coverage_by_platform[platform] = platform_tests

                generated_tests_root_node = ActionNode.CreateRootNode()
                build_action_node_graph(generated_tests_root_node,
                                        platform_tests)
                trim_graph_to_platform_actions(generated_tests_root_node,
                                               platform)
                generated_tests_by_platform[
                    platform] = generate_framework_tests(
                        generated_tests_root_node, platform)

            required_coverage_by_platform_set: CoverageTestsByPlatformSet = (
                partition_framework_tests_per_platform_combination(
                    generated_tests_by_platform))
            for platform_set, tests in required_coverage_by_platform_set.items(
            ):
                expected_filename = os.path.join(
                    test_partition.browsertest_dir,
                    test_partition.test_file_prefix)
                if len(platform_set) != len(TestPlatform):
                    for platform in TestPlatform:
                        if platform in platform_set:
                            expected_filename += "_" + platform.suffix
                expected_filename += ".txt"
                with open(expected_filename, "r",
                          encoding="utf-8") as expected_tests_file:
                    expected_tests_str = expected_tests_file.read()
                    actual_tests_str = "\n".join([
                        test.generate_browsertest(test_partition)
                        for test in tests
                    ])
                    self.assertEqual(expected_tests_str, actual_tests_str)


if __name__ == '__main__':
    unittest.main()
