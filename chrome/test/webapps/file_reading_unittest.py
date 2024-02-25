#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import csv
import tempfile
from typing import List
import shutil
import unittest

from file_reading import enumerate_all_argument_combinations
from file_reading import expand_tests_from_action_parameter_wildcards
from file_reading import enumerate_markdown_file_lines_to_table_rows
from file_reading import human_friendly_name_to_canonical_action_name
from file_reading import generate_test_id_from_test_steps
from file_reading import get_and_maybe_delete_tests_in_browsertest
from file_reading import read_actions_file
from file_reading import read_enums_file
from file_reading import read_platform_supported_actions
from file_reading import resolve_bash_style_replacement
from file_reading import read_unprocessed_coverage_tests_file
from models import ActionsByName
from models import ArgEnum
from models import CoverageTest
from models import TestIdTestNameTuple
from models import TestPlatform

TEST_DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "test_data")


class TestAnalysisTest(unittest.TestCase):
    def test_markdown_file_mapping(self):
        test_input = [
            "Test", "# Hello", "| #test |", "| ------- | Value |",
            "| WML | Value | Value", " | ADF |"
        ]
        output = enumerate_markdown_file_lines_to_table_rows(test_input)
        expected = [(4, ["WML", "Value", "Value"]), (5, ["ADF"])]
        self.assertEqual(expected, output)

    def test_argument_combinations(self):
        argument_types: List[ArgEnum] = []
        argument_types.append(ArgEnum("T1", ["T1V1", "T1V2"], None))
        argument_types.append(ArgEnum("T2", ["T2V1", "T2V2"], None))

        combinations = enumerate_all_argument_combinations(argument_types)

        self.assertEqual([["T1V1", "T2V1"], ["T1V1", "T2V2"], ["T1V2", "T2V1"],
                          ["T1V2", "T2V2"]], combinations)

    def test_resolving_output_action_names(self):
        self.assertEqual("Test",
                         resolve_bash_style_replacement("$1", ["Test"]))
        self.assertEqual(
            "Correct sentence! over over.",
            resolve_bash_style_replacement("$1 $2ence! $3 $3.",
                                           ["Correct", "sent", "over"]))

    def test_human_friendly_name_to_canonical_action_name(self):
        self.assertEqual(
            "action_with_arg1_arg2",
            human_friendly_name_to_canonical_action_name(
                "action_with(arg1, arg2)", {}))
        self.assertEqual(
            "action_with_arg1_arg2",
            human_friendly_name_to_canonical_action_name(
                "action_with", {"action_with": "arg1_arg2"}))

    def test_enums(self):
        enums_file = os.path.join(TEST_DATA_DIR, "test_enums.md")

        with open(enums_file, "r", encoding="utf-8") \
                    as enum_types:
            enums = read_enums_file(enum_types.readlines())
            self.assertEqual(len(enums), 3)
            self.assertIn("Animal", enums)
            animal = enums["Animal"]
            self.assertEqual("Animal", animal.type_name)
            self.assertEqual(["Chicken", "Dog"], animal.values)
            self.assertEqual("Chicken", animal.default_value)

            self.assertIn("AnimalLess", enums)
            animal = enums["AnimalLess"]
            self.assertEqual("AnimalLess", animal.type_name)
            self.assertEqual(["Chicken"], animal.values)
            self.assertEqual("Chicken", animal.default_value)

            self.assertIn("Color", enums)
            color = enums["Color"]
            self.assertEqual("Color", color.type_name)
            self.assertEqual(["Green", "Red"], color.values)
            self.assertEqual(None, color.default_value)

    def test_supported_actions(self):
        supported_actions_filename = os.path.join(
            TEST_DATA_DIR, "framework_supported_actions.csv")

        with open(supported_actions_filename, "r", encoding="utf-8") \
                    as supported_actions:
            supported = read_platform_supported_actions(
                csv.reader(supported_actions, delimiter=','))
            self.assertEqual(len(supported), 4)
            (check_a_partial, check_a_full) = supported["check_a"]
            self.assertEqual(len(check_a_partial), 1)
            self.assertEqual(len(check_a_full), 3)
            (check_b_partial, check_b_full) = supported["check_b"]
            self.assertEqual(len(check_b_partial), 1)
            self.assertEqual(len(check_b_full), 3)
            (state_change_a_partial,
             state_change_a_full) = supported["state_change_a"]
            self.assertEqual(len(state_change_a_partial), 0)
            self.assertEqual(len(state_change_a_full), 4)
            (state_change_b_partial,
             state_change_b_full) = supported["state_change_b"]
            self.assertEqual(len(state_change_b_partial), 0)
            self.assertEqual(len(state_change_b_full), 3)

    def test_action_file_reading(self):
        actions_filename = os.path.join(TEST_DATA_DIR, "test_actions.md")
        supported_actions_filename = os.path.join(
            TEST_DATA_DIR, "framework_supported_actions.csv")
        enums_filename = os.path.join(TEST_DATA_DIR, "test_enums.md")

        with open(actions_filename, "r", encoding="utf-8") as f, \
                open(supported_actions_filename, "r", encoding="utf-8") \
                    as supported_actions, \
                open (enums_filename, "r", encoding="utf-8") as enums:
            supported_actions = read_platform_supported_actions(
                csv.reader(supported_actions, delimiter=','))
            actions_tsv = f.readlines()
            enums = read_enums_file(enums.readlines())

            (actions, action_base_name_to_default_param) = read_actions_file(
                actions_tsv, enums, supported_actions)
            self.assertEqual(len(actions), 13)
            self.assertEqual(len(action_base_name_to_default_param), 3)

            # Check Cpp methods
            self.assertIn('check_b_Chicken_Green', actions)
            self.assertEqual("CheckB(Animal::kChicken, Color::kGreen)",
                             actions['check_b_Chicken_Green'].cpp_method)

            # Check parameterized action state.
            self.assertIn('changes_Chicken', actions)
            self.assertIn('changes_Dog', actions)
            self.assertTrue('checks' in actions)
            checks_output_actions = actions['checks'].output_actions
            self.assertEqual(len(checks_output_actions), 2)
            self.assertCountEqual(
                checks_output_actions,
                [actions['check_a_Chicken'], actions['check_b_Chicken_Green']])

    def test_coverage_file_reading(self):
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
                                         "test_unprocessed_coverage.md")
        coverage_tests: List[CoverageTest] = []
        with open(coverage_filename) as f:
            coverage_tsv = f.readlines()
            coverage_tests = read_unprocessed_coverage_tests_file(
                coverage_tsv, actions, enums,
                action_base_name_to_default_param)
        self.assertEqual(6, len(coverage_tests))

    def test_browsertest_detection(self):
        browsertest_filename = os.path.join(TEST_DATA_DIR, "tests_default.cc")
        tests_and_platforms = get_and_maybe_delete_tests_in_browsertest(
            browsertest_filename)
        expected_key = TestIdTestNameTuple(
            "state_change_a_Chicken_check_a_Chicken_check_b_Chicken_Green",
            "3Chicken_1Chicken_2ChickenGreen")
        self.assertListEqual(list(tests_and_platforms.keys()), [expected_key])
        tests_and_platforms = tests_and_platforms[expected_key]
        self.assertEqual(
            {TestPlatform.LINUX, TestPlatform.CHROME_OS, TestPlatform.MAC},
            tests_and_platforms)

    def test_browertest_in_place_deletion(self):
        input_file = os.path.join(TEST_DATA_DIR, "tests_for_deletion.cc")
        after_deletion_file = os.path.join(TEST_DATA_DIR, "tests_default.cc")
        with tempfile.TemporaryDirectory(dir=TEST_DATA_DIR) as tmpdirname:
            output_file = os.path.join(tmpdirname, "output.cc")
            shutil.copyfile(input_file, output_file)
            tests_and_platforms = get_and_maybe_delete_tests_in_browsertest(
                output_file, {
                    TestIdTestNameTuple(
                        "state_change_a_Chicken_check_a_Chicken_check_b_Chicken_Green",
                        "StateChangeAChicken")
                },
                delete_in_place=True)

            with open(output_file, 'r') as f, open(after_deletion_file,
                                                   'r') as f2:
                self.assertTrue(f.read(), f2.read())

            tests_and_platforms = tests_and_platforms[TestIdTestNameTuple(
                "state_change_a_Chicken_check_a_Chicken_check_b_Chicken_Green",
                "3Chicken_1Chicken_2ChickenGreen")]
            self.assertEqual(
                {TestPlatform.LINUX, TestPlatform.CHROME_OS, TestPlatform.MAC},
                tests_and_platforms)

    def test_action_param_expansion(self):
        enum_map: Dict[str, ArgEnum] = {
            "EnumType": ArgEnum("EnumType", ["Value1", "Value2"], None)
        }
        actions: List[str] = [
            "Action1(EnumType::All)", "Action2(EnumType::All, EnumType::All)"
        ]

        combinations = expand_tests_from_action_parameter_wildcards(
            enum_map, actions)
        expected = [['Action1(Value1)', 'Action2(Value1, Value1)'],
                    ['Action1(Value2)', 'Action2(Value1, Value1)'],
                    ['Action1(Value1)', 'Action2(Value1, Value2)'],
                    ['Action1(Value2)', 'Action2(Value1, Value2)'],
                    ['Action1(Value1)', 'Action2(Value2, Value1)'],
                    ['Action1(Value2)', 'Action2(Value2, Value1)'],
                    ['Action1(Value1)', 'Action2(Value2, Value2)'],
                    ['Action1(Value2)', 'Action2(Value2, Value2)']]
        self.assertCountEqual(combinations, expected)

    def test_generate_test_id_from_test_steps(self):
        test_steps = [
            "helper_.StateChangeA(Animal::kChicken);",
            "helper_.CheckB(Animal::kChicken, Color::kGreen);",
            "helper_.StateChangeB();"
        ]
        test_id = generate_test_id_from_test_steps(test_steps)
        expected_test_id = (
            "state_change_a_Chicken_check_b_Chicken_Green_state_change_b"
        )
        self.assertEqual(test_id, expected_test_id)


if __name__ == '__main__':
    unittest.main()
