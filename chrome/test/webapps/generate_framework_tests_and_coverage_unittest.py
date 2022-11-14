#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from io import StringIO
import os
import sys
from typing import Dict
import unittest
import tempfile

from generate_framework_tests_and_coverage import generate_framework_tests_and_coverage
from models import TestPartitionDescription
from models import TestPlatform

TEST_DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "test_data")


class GenerateFrameworkTestsAndCoverageTest(unittest.TestCase):
    def test_coverage(self):
        actions_filename = os.path.join(TEST_DATA_DIR, "test_actions.md")
        enums_filename = os.path.join(TEST_DATA_DIR, "test_enums.md")
        supported_actions_filename = os.path.join(
            TEST_DATA_DIR, "framework_supported_actions.csv")

        coverage_filename = os.path.join(TEST_DATA_DIR,
                                         "test_unprocessed_coverage.md")

        custom_partitions = [
            TestPartitionDescription(
                action_name_prefixes={"state_change_b"},
                browsertest_dir=TEST_DATA_DIR,
                test_file_prefix="tests_change_b",
                test_fixture="TwoClientWebAppsIntegrationSyncTest")
        ]
        default_partition = TestPartitionDescription(
            action_name_prefixes=set(),
            browsertest_dir=TEST_DATA_DIR,
            test_file_prefix="tests_default",
            test_fixture="WebAppIntegrationTest")

        with open(actions_filename, "r", encoding="utf-8") as actions_file, \
                open(supported_actions_filename, "r", encoding="utf-8") \
                    as supported_actions_file, \
                open(coverage_filename, "r", encoding="utf-8") \
                    as coverage_file, \
                open(enums_filename, "r", encoding="utf-8") as enums, \
                tempfile.TemporaryDirectory() as output_dir:
            capturedOutput = StringIO()
            sys.stdout = capturedOutput
            generate_framework_tests_and_coverage(
                supported_actions_file, enums, actions_file, coverage_file,
                custom_partitions, default_partition, output_dir, None)
            # The framework uses stdout to inform the developer of tests that
            # need to be added or removed. Since there should be no tests
            # changes required, nothing should be printed to stdout.
            self.assertFalse(capturedOutput.read())
            sys.stdout = sys.__stdout__

            for platform in TestPlatform:
                file_title = "coverage_" + platform.suffix + ".tsv"
                gen_coverage_filename = os.path.join(output_dir, file_title)
                expected_coverage_filename = os.path.join(
                    TEST_DATA_DIR, "expected_" + file_title)
                with open(gen_coverage_filename, "r", encoding="utf-8") \
                        as coverage_file, \
                        open(expected_coverage_filename, "r", \
                        encoding="utf-8") as expected_file:
                    self.assertListEqual(
                        list(expected_file.readlines()),
                        list(coverage_file.readlines()),
                        f"file: {expected_coverage_filename}")


if __name__ == '__main__':
    unittest.main()
