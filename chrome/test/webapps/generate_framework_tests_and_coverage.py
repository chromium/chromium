#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script used to generate the tests definitions for Web App testing framework.
See the README.md file in this directory for more information.

Usage: python3 chrome/test/web_apps/generate_framework_tests_and_coverage.py
"""

import argparse
import logging
import os
import csv

from graph_analysis import AddPartialPaths
from graph_analysis import CreateFullCoverageActionGraph
from graph_analysis import GenerateFrameworkTests
from graph_analysis import GenerateGraphvizDotFile
from graph_analysis import GenerateCoverageFileAndPercents
from graph_analysis import TrimGraphToAllowedActions
from classes import ActionCoverage
from classes import Action
from classes import ActionNode
from classes import CoverageTest
from classes import PartialCoverageAddition
from file_reading import ReadActionsFile
from file_reading import ReadCoverageTestsFile
from file_reading import ReadFrameworkActions
from file_reading import ReadPartialCoveragePathsFile
from file_reading import ReadNamedTestsFile

# These actions, if detected, will add a test to a separate file.
SYNC_ACTIONS = [
    "switch_profile_clients_user_a_client_1",
    "switch_profile_clients_user_a_client_2", "sync_turn_on", "sync_turn_off"
]

GENERATED_FILE_HEADER = ("# DO NOT EDIT - THIS IS A GENERATED FILE.\n" +
                         "# See /chrome/test/web_app/README.md for more info.")


def main():
    parser = argparse.ArgumentParser(description='WebApp Test List Processor')
    parser.add_argument('-v',
                        dest='v',
                        action='store_true',
                        help='Include info logging.',
                        required=False)

    parser.add_argument('--graphs',
                        dest='graphs',
                        action='store_true',
                        help='Output dot graphs from all steps.',
                        required=False)
    parser.add_argument(
        '--ignore_audited',
        dest='ignore_audited',
        action='store_true',
        help='Ignore the audited manual and automated test data',
        required=False)
    options = parser.parse_args()
    logging.basicConfig(level=logging.INFO if options.v else logging.WARN,
                        format='[%(asctime)s %(levelname)s] %(message)s',
                        datefmt='%H:%M:%S')
    script_dir = os.path.dirname(os.path.realpath(__file__))
    output_dir = script_dir + "/output"
    actions_file = script_dir + "/data/actions.csv"
    coverage_required_file = script_dir + "/data/coverage_required.csv"
    partial_coverage_file = script_dir + "/data/partial_coverage_paths.csv"
    audited_manual_tests_file = script_dir + "/data/manual_tests.csv"
    audited_automated_tests_file = script_dir + "/data/automated_tests.csv"
    partial_coverage_file = script_dir + "/data/partial_coverage_paths.csv"
    framework_actions_file_base = script_dir + "/data/framework_actions_"

    actions_csv = csv.reader(open(actions_file), delimiter=',')
    coverage_csv = csv.reader(open(coverage_required_file), delimiter=',')
    partial_csv = csv.reader(open(partial_coverage_file), delimiter=',')

    (actions, action_base_name_to_default_param,
     action_base_name_to_all_params) = ReadActionsFile(actions_csv)
    required_coverage_tests = ReadCoverageTestsFile(
        coverage_csv, actions, action_base_name_to_default_param)
    partial_paths = ReadPartialCoveragePathsFile(
        partial_csv, actions, action_base_name_to_all_params)
    partial_csv = csv.reader(open(partial_coverage_file), delimiter=',')
    reversed_partial_paths = ReadPartialCoveragePathsFile(
        partial_csv, actions, action_base_name_to_all_params)
    for path in reversed_partial_paths:
        path.Reverse()

    if options.graphs:
        coverage_root_node = ActionNode(Action("root", "root", False))
        CreateFullCoverageActionGraph(coverage_root_node,
                                      required_coverage_tests)
        AddPartialPaths(coverage_root_node, partial_paths)
        coverage_graph = GenerateGraphvizDotFile(coverage_root_node)
        output_coverage_graph_file_name = (output_dir +
                                           "/coverage_required_graph.dot")
        coverage_graph_file = open(output_coverage_graph_file_name, 'w')
        coverage_graph_file.write(GENERATED_FILE_HEADER + "\n")
        coverage_graph_file.write(coverage_graph)
        coverage_graph_file.close()

    for platform in ['linux', 'mac', 'cros', 'win']:
        framework_actions_csv = csv.reader(open(framework_actions_file_base +
                                                platform + ".csv"),
                                           delimiter=',')
        framework_actions = ReadFrameworkActions(
            framework_actions_csv, actions, action_base_name_to_default_param)

        # Create the coverage graph, then prune all unsupported actions for
        # this platform.
        coverage_root_node = ActionNode(Action("root", "root", False))
        CreateFullCoverageActionGraph(coverage_root_node,
                                      required_coverage_tests)
        AddPartialPaths(coverage_root_node, partial_paths)
        TrimGraphToAllowedActions(coverage_root_node, framework_actions)

        if options.graphs:
            graph = GenerateGraphvizDotFile(coverage_root_node)
            output_graph_file_name = (output_dir + "/framework_test_graph_" +
                                      platform + ".dot")
            graph_file = open(output_graph_file_name, 'w')
            graph_file.write(GENERATED_FILE_HEADER + "\n")
            graph_file.write(graph)
            graph_file.close()

        # Write the framework tests. All tests that involve 'sync' actions must
        # be in a separate file.
        output_tests_file_name = (output_dir + "/framework_tests_" + platform +
                                  ".csv")
        output_tests_sync_file_name = (output_dir + "/framework_tests_sync_" +
                                       platform + ".csv")
        framework_file = open(output_tests_file_name, 'w')
        framework_sync_file = open(output_tests_sync_file_name, 'w')
        framework_file.write(GENERATED_FILE_HEADER + "\n")
        framework_sync_file.write(GENERATED_FILE_HEADER + "\n")
        paths = GenerateFrameworkTests(coverage_root_node,
                                       required_coverage_tests)
        for path in paths:
            all_actions_in_path = []
            for node in path[1:]:  # Skip the root node
                all_actions_in_path.append(node.action)
                all_actions_in_path.extend(node.state_check_actions.values())
            action_names = [action.name for action in all_actions_in_path]
            file = framework_file
            for action_name in action_names:
                if action_name in SYNC_ACTIONS:
                    file = framework_sync_file
                    break
            file.write("," + ",".join(action_names) + "\n")
        framework_file.close()
        framework_sync_file.close()

        # Analyze all tests to generate coverage
        test_files = [output_tests_file_name, output_tests_sync_file_name]
        if not options.ignore_audited:
            test_files.extend(
                [audited_automated_tests_file, audited_manual_tests_file])
        tests = []
        for tests_file in test_files:
            tests_csv = csv.reader(open(tests_file), delimiter=',')
            tests.extend(
                ReadNamedTestsFile(tests_csv, actions,
                                   action_base_name_to_default_param))
        tests_root_node = ActionNode(Action("root", "root", False))
        CreateFullCoverageActionGraph(tests_root_node, tests)
        AddPartialPaths(tests_root_node, reversed_partial_paths)

        (coverage_file_text, full_coverage,
         partial_coverage) = GenerateCoverageFileAndPercents(
             required_coverage_tests, tests_root_node)

        coverage_table_file = output_dir + "/coverage_" + platform + ".tsv"
        coverage_file = open(coverage_table_file, 'w')
        coverage_file.write(GENERATED_FILE_HEADER + "\n")
        coverage_file.write(
            "# Full Coverage: {:.2f}, Partial Coverage: {:.2f}\n".format(
                full_coverage, partial_coverage))
        coverage_file.write(coverage_file_text)
        coverage_file.close()

        if options.graphs:
            coverage_graph = GenerateGraphvizDotFile(tests_root_node)
            coverage_graph_filename = (output_dir + "/coverage_graph_" +
                                       platform + ".dot")
            coverage_graph_file = open(coverage_graph_filename, 'w')
            coverage_graph_file.write(GENERATED_FILE_HEADER + "\n")
            for graph_line in coverage_graph:
                coverage_graph_file.write(graph_line + "\n")
            coverage_graph_file.close()


if __name__ == '__main__':
    main()
