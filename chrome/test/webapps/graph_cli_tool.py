#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command line tool to analyze and debug the action graph algorithms.
"""

import argparse
import csv
import logging
from typing import List
import os

from models import TestPartitionDescription
from models import ActionNode
from models import CoverageTest
from models import TestPlatform
from file_reading import get_tests_in_browsertest
from file_reading import read_actions_file
from file_reading import read_unprocessed_coverage_tests_file
from graph_analysis import build_action_node_graph
from graph_analysis import generate_framework_tests
from graph_analysis import generage_graphviz_dot_file
from graph_analysis import trim_graph_to_platform_actions
from test_analysis import expand_parameterized_tests
from test_analysis import filter_coverage_tests_for_platform
from test_analysis import partition_framework_tests_per_platform_combination


def MaybeFilterCoverageTests(coverage_tests: List[CoverageTest],
                             filter: List[str]) -> List[CoverageTest]:
    if not filter:
        return coverage_tests
    return [coverage_tests[int(index_as_string)] for index_as_string in filter]


def main():
    parser = argparse.ArgumentParser(
        description='WebApp Integration Test Analysis CLI Tool')
    parser.add_argument('-v',
                        dest='v',
                        action='store_true',
                        help='Include info logging.',
                        required=False)

    script_dir = os.path.dirname(os.path.realpath(__file__))

    parser.add_argument(
        '--test_dir',
        dest='test_dir',
        action='store',
        help=('Specify a directory to find all required files, instead of ' +
              'specifying each file individually. Overrides those options.'),
        required=False)

    parser.add_argument(
        '--coverage_required',
        dest='coverage_required',
        action='store',
        default=(script_dir + '/data/coverage_required.csv'),
        help=(
            'Test list csv file, which lists all integration tests that would '
            + 'give the required full coverage of the system. The first two ' +
            'lines are skipped.'),
        required=False)
    parser.add_argument('--coverage_test_row',
                        dest='coverage_test_row',
                        action='append',
                        help='Individually select a coverage test row.',
                        required=False)

    parser.add_argument('--actions',
                        dest='actions',
                        action='store',
                        default=(script_dir + '/data/actions.csv'),
                        help='Actions csv file, defining all actions.',
                        required=False)

    parser.add_argument('--framework_actions',
                        dest='framework_actions',
                        default=(script_dir +
                                 '/data/framework_actions_linux.csv'),
                        help=('Framework actions csv file, enumerating ' +
                              'all actions supported by the framework'),
                        action='store',
                        required=False)
    parser.add_argument(
        '--tests',
        dest='tests',
        action='append',
        help=('Test csv files, enumerating all existing tests for coverage ' +
              'calculations. First column is expected to be the test name.'),
        required=False)

    subparsers = parser.add_subparsers(dest="cmd", required=True)
    subparsers.add_parser('list_actions')
    subparsers.add_parser('list_coverage_tests')
    subparsers.add_parser('list_processed_coverage_tests')
    subparsers.add_parser('coverage_required_graph')

    framework_parse = subparsers.add_parser(
        'generate_framework_tests_for_platform')
    framework_parse.add_argument('--platform',
                                 dest='platform',
                                 required=True,
                                 choices=["M", "W", "L", "C"],
                                 action="store")
    framework_parse.add_argument('--graph_framework_tests',
                                 dest='graph_framework_tests',
                                 default=False,
                                 action='store_true')

    subparsers.add_parser('print_all_framework_tests')

    save_files_parse = subparsers.add_parser(
        'save_or_modify_framework_test_files')
    save_files_parse.add_argument('--dir',
                                  dest='dir',
                                  default='.',
                                  action="store")
    save_files_parse.add_argument('--base_filename',
                                  dest='base_filename',
                                  default='web_app_integration_browsertest',
                                  action="store")

    subparsers.add_parser('generate_test_coverage')

    options = parser.parse_args()

    actions_file = options.actions
    coverage_required_file = options.coverage_required

    if options.test_dir:
        actions_file = options.test_dir + "/actions.csv"
        coverage_required_file = options.test_dir + "/coverage_required.csv"

    logging.basicConfig(level=logging.INFO if options.v else logging.WARN,
                        format='[%(asctime)s %(levelname)s] %(message)s',
                        datefmt='%H:%M:%S')

    logging.info('Script directory: ' + script_dir)

    actions_csv = csv.reader(open(actions_file, "r", encoding="utf-8"),
                             delimiter=',')
    (actions,
     action_base_name_to_default_param) = read_actions_file(actions_csv)

    default_partition = TestPartitionDescription([], os.path.join(script_dir),
                                                 "test_browsertest",
                                                 "WebAppIntegrationTestBase")

    if options.cmd == 'list_actions':
        for action in actions.values():
            print(action)
        return
    if options.cmd == 'list_coverage_tests':
        coverage_csv = csv.reader(open(coverage_required_file,
                                       "r",
                                       encoding="utf-8"),
                                  delimiter=',')
        required_coverage_tests = read_unprocessed_coverage_tests_file(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        for test in required_coverage_tests:
            print(test if options.v else test.id)
        return
    if options.cmd == 'list_processed_coverage_tests':
        coverage_csv = csv.reader(open(coverage_required_file,
                                       "r",
                                       encoding="utf-8"),
                                  delimiter=',')
        required_coverage_tests = read_unprocessed_coverage_tests_file(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        required_coverage_tests = expand_parameterized_tests(
            required_coverage_tests)
        for test in required_coverage_tests:
            print(test if options.v else test.name)
        return
    if options.cmd == 'coverage_required_graph':
        coverage_csv = csv.reader(open(coverage_required_file,
                                       "r",
                                       encoding="utf-8"),
                                  delimiter=',')
        required_coverage_tests = read_unprocessed_coverage_tests_file(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        required_coverage_tests = expand_parameterized_tests(
            required_coverage_tests)
        coverage_root_node = ActionNode.CreateRootNode()
        build_action_node_graph(coverage_root_node, required_coverage_tests)
        graph_file = generage_graphviz_dot_file(coverage_root_node)
        print(graph_file)
        return
    if options.cmd == 'generate_framework_tests_for_platform':
        coverage_csv = csv.reader(open(coverage_required_file,
                                       "r",
                                       encoding="utf-8"),
                                  delimiter=',')
        required_coverage_tests = read_unprocessed_coverage_tests_file(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        required_coverage_tests = expand_parameterized_tests(
            required_coverage_tests)
        platform_lookup = {
            "M": TestPlatform.MAC,
            "W": TestPlatform.WINDOWS,
            "L": TestPlatform.LINUX,
            "C": TestPlatform.CHROME_OS
        }
        platform = platform_lookup[options.platform]
        required_coverage_tests = filter_coverage_tests_for_platform(
            required_coverage_tests, platform)
        coverage_root_node = ActionNode.CreateRootNode()
        build_action_node_graph(coverage_root_node, required_coverage_tests)
        trim_graph_to_platform_actions(coverage_root_node, platform)
        if options.graph_framework_tests:
            return generage_graphviz_dot_file(coverage_root_node)
        lines = []
        tests = generate_framework_tests(coverage_root_node)
        for test in tests:
            lines.append(test.GenerateBrowsertest(default_partition))
        print("\n".join(lines))
        return
    if options.cmd == 'print_all_framework_tests':
        coverage_csv = csv.reader(open(coverage_required_file,
                                       "r",
                                       encoding="utf-8"),
                                  delimiter=',')
        required_coverage_tests = read_unprocessed_coverage_tests_file(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        required_coverage_tests = expand_parameterized_tests(
            required_coverage_tests)
        platform_set_to_tests = partition_framework_tests_per_platform_combination(
            required_coverage_tests)

        for platforms, tests in platform_set_to_tests.items():
            print(f"\n\n\nTests for {platforms!r}!")
            for test in tests:
                print(test.generate_browsertest(default_partition))
        return
    if options.cmd == 'save_or_modify_framework_test_files':
        coverage_csv = csv.reader(open(coverage_required_file,
                                       "r",
                                       encoding="utf-8"),
                                  delimiter=',')
        required_coverage_tests = read_unprocessed_coverage_tests_file(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        required_coverage_tests = expand_parameterized_tests(
            required_coverage_tests)
        platform_set_to_tests = partition_framework_tests_per_platform_combination(
            required_coverage_tests)

        existing_tests_by_platform_set = {}

        for file in os.listdir(options.dir):
            if not file.startswith(options.base_filename):
                continue
            platforms = TestPlatform.get_platforms_from_browsertest_filename(
                file)
            platforms = frozenset(platforms)
            with open(options.dir + os.path.sep + file, "r",
                      encoding="utf-8") as f:
                file = f.read()
                tests = get_tests_in_browsertest(file)
                existing_tests_by_platform_set[platforms] = list(tests.keys())

        for platforms, tests in platform_set_to_tests.items():
            tests_to_add = []
            for test in tests:
                if platforms in existing_tests_by_platform_set:
                    existing_tests = existing_tests_by_platform_set[platforms]
                    if test.name not in existing_tests:
                        tests_to_add.append(test)
                    else:
                        existing_tests.remove(test.name)
                else:
                    tests_to_add.append(test)
            if not tests_to_add:
                continue
            print(f"\n\nAdd this following tests to "
                  f"{default_partition.generate_test_filename(platforms)}:\n")
            for test in tests_to_add:
                print(test.GenerateBrowsertest(default_partition) + "\n")

        for platforms, test_names in existing_tests_by_platform_set.items():
            if not test_names:
                continue
            print(f"\n\nRemove this following tests from "
                  f"{default_partition.generate_test_filename(platforms)}: "
                  f"{', '.join(test_names)}")
        return


if __name__ == '__main__':
    main()
