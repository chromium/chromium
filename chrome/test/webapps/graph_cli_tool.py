#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command line tool to analyze and debug the action graph algorithms.
"""

import argparse
import csv
import logging
from typing import List, Set, Tuple
import os

from classes import Action
from classes import ActionNode
from classes import CoverageTest
from file_reading import ReadActionsFile
from file_reading import ReadCoverageTestsFile
from file_reading import ReadFrameworkActions
from file_reading import ReadPartialCoveragePathsFile
from file_reading import ReadNamedTestsFile
from graph_analysis import CreateFullCoverageActionGraph
from graph_analysis import AddPartialPaths
from graph_analysis import GenerateCoverageFileAndPercents
from graph_analysis import GenerateFrameworkTests
from graph_analysis import GenerateGraphvizDotFile
from graph_analysis import TrimGraphToAllowedActions


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

    parser.add_argument(
        '--partial_coverage_paths',
        dest='partial_coverage_paths',
        action='store',
        default=(script_dir + '/data/partial_coverage_paths.csv'),
        help=('File with path replacement descriptions to create partial ' +
              'coverage paths in the tree.'),
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
        help=(
            'Test csv files, enumerating all existing tests for coverage ' +
            'calculations. First line is skipped, and first column is skipped.'
        ),
        required=False)

    subparsers = parser.add_subparsers(dest="cmd", required=True)
    subparsers.add_parser('list_actions')
    subparsers.add_parser('list_coverage_tests')
    subparsers.add_parser('list_tests')
    subparsers.add_parser('list_partial_paths')
    subparsers.add_parser('coverage_required_graph')

    framework_parse = subparsers.add_parser('generate_framework_tests')
    framework_parse.add_argument('--graph_framework_tests',
                                 dest='graph_framework_tests',
                                 default=False,
                                 action='store_true')

    coverage_parse = subparsers.add_parser('generate_test_coverage')
    coverage_parse.add_argument('--graph_test_coverage',
                                dest='graph_test_coverage',
                                default=False,
                                action='store_true')

    options = parser.parse_args()

    actions_file = options.actions
    coverage_required_file = options.coverage_required
    partial_paths_file = options.partial_coverage_paths
    framework_actions_file = options.framework_actions

    if options.test_dir:
        actions_file = options.test_dir + "/actions.csv"
        coverage_required_file = options.test_dir + "/coverage_required.csv"
        partial_paths_file = options.test_dir + "/partial_coverage_paths.csv"
        framework_actions_file = (options.test_dir +
                                  "/framework_actions_linux.csv")

    logging.basicConfig(level=logging.INFO if options.v else logging.WARN,
                        format='[%(asctime)s %(levelname)s] %(message)s',
                        datefmt='%H:%M:%S')

    logging.info('Script directory: ' + script_dir)

    actions_csv = csv.reader(open(actions_file), delimiter=',')
    (actions, action_base_name_to_default_param,
     action_base_name_to_all_params) = ReadActionsFile(actions_csv)

    if options.cmd == 'list_actions':
        for action in actions.values():
            print(action)
        return
    if options.cmd == 'list_tests':
        tests = []
        for tests_file in options.tests:
            tests_csv = csv.reader(open(tests_file), delimiter=',')
            tests.extend(
                ReadNamedTestsFile(tests_csv, actions,
                                   action_base_name_to_default_param))
        for test in tests:
            print(test)
        return
    if options.cmd == 'list_coverage_tests':
        coverage_csv = csv.reader(open(coverage_required_file), delimiter=',')
        required_coverage_tests = ReadCoverageTestsFile(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        for test in required_coverage_tests:
            print(test)
        return
    if options.cmd == 'list_partial_paths':
        partial_csv = csv.reader(open(partial_paths_file), delimiter=',')
        partial_paths = ReadPartialCoveragePathsFile(
            partial_csv, actions, action_base_name_to_all_params)
        for partial_path in partial_paths:
            print(partial_path)
        return
    if options.cmd == 'coverage_required_graph':
        coverage_csv = csv.reader(open(coverage_required_file), delimiter=',')
        required_coverage_tests = ReadCoverageTestsFile(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        coverage_root_node = ActionNode(Action("root", "root", False))
        CreateFullCoverageActionGraph(coverage_root_node,
                                      required_coverage_tests)
        partial_csv = csv.reader(open(partial_paths_file), delimiter=',')
        partial_paths = ReadPartialCoveragePathsFile(
            partial_csv, actions, action_base_name_to_all_params)
        AddPartialPaths(coverage_root_node, partial_paths)
        graph_file = GenerateGraphvizDotFile(coverage_root_node)
        print(graph_file)
        return
    if options.cmd == 'generate_framework_tests':
        coverage_csv = csv.reader(open(coverage_required_file), delimiter=',')
        framework_actions_csv = csv.reader(open(framework_actions_file),
                                           delimiter=',')
        required_coverage_tests = ReadCoverageTestsFile(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        framework_actions = ReadFrameworkActions(
            framework_actions_csv, actions, action_base_name_to_default_param)
        coverage_root_node = ActionNode(Action("root", "root", False))
        CreateFullCoverageActionGraph(coverage_root_node,
                                      required_coverage_tests)
        partial_csv = csv.reader(open(partial_paths_file), delimiter=',')
        partial_paths = ReadPartialCoveragePathsFile(
            partial_csv, actions, action_base_name_to_all_params)
        AddPartialPaths(coverage_root_node, partial_paths)

        TrimGraphToAllowedActions(coverage_root_node, framework_actions)
        if options.graph_framework_tests:
            return GenerateGraphvizDotFile(coverage_root_node)

        lines = ["# This is a generated file."]
        paths = GenerateFrameworkTests(coverage_root_node,
                                       required_coverage_tests)
        for path in paths:
            all_actions_in_path = []
            for node in path[1:]:  # Skip the root node
                all_actions_in_path.append(node.action)
                all_actions_in_path.extend(node.state_check_actions.values())
            lines.append(
                "," + ",".join([action.name
                                for action in all_actions_in_path]))
        print("\n".join(lines))
        return
    if options.cmd == 'generate_test_coverage':
        coverage_csv = csv.reader(open(coverage_required_file), delimiter=',')
        required_coverage_tests = ReadCoverageTestsFile(
            coverage_csv, actions, action_base_name_to_default_param)
        required_coverage_tests = MaybeFilterCoverageTests(
            required_coverage_tests, options.coverage_test_row)
        partial_csv = csv.reader(open(partial_paths_file), delimiter=',')
        partial_paths = ReadPartialCoveragePathsFile(
            partial_csv, actions, action_base_name_to_all_params)
        for path in partial_paths:
            path.Reverse()
        tests = []
        for tests_file in options.tests:
            tests_csv = csv.reader(open(tests_file), delimiter=',')
            tests.extend(
                ReadNamedTestsFile(tests_csv, actions,
                                   action_base_name_to_default_param))
        tests_root_node = ActionNode(Action("root", "root", False))
        CreateFullCoverageActionGraph(tests_root_node, tests)
        AddPartialPaths(tests_root_node, partial_paths)

        if options.graph_test_coverage:
            print(GenerateGraphvizDotFile(tests_root_node))
            return
        (coverage_file, _,
         _) = GenerateCoverageFileAndPercents(required_coverage_tests,
                                              tests_root_node)
        print(coverage_file)


if __name__ == '__main__':
    main()
