#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test analysis functions for the testing framework.
"""

from collections import defaultdict
import logging
import os
import re
from typing import Dict, List, Set

from models import Action
from models import ActionType
from models import CoverageTest
from models import CoverageTestsByPlatform
from models import CoverageTestsByPlatformSet
from models import TestId
from models import TestIdsTestNamesByPlatformSet
from models import TestPartitionDescription
from models import TestPlatform


def filter_tests_for_partition(tests: List[CoverageTest],
                               partition: TestPartitionDescription
                               ) -> List[CoverageTest]:
    """Returns tests whose actions match any prefix assigned to `partition`."""
    return [
        test for test in tests if any(
            action.name.startswith(prefix) for action in test.actions
            for prefix in partition.action_name_prefixes)
    ]


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

    def write_tests(filename: str, tests: List[CoverageTest],
                    partition: TestPartitionDescription,
                    ordered_tests: List[CoverageTest]):

        def update_existing_generated_test_block(existing_block: str,
                                                 new_block: str) -> str:
            """Replace helper_ body; preserve existing header when possible."""
            existing_body_match = re.search(r'(?m)^[ \t]*helper_\.',
                                            existing_block)
            new_body_match = re.search(r'(?m)^[ \t]*helper_\.', new_block)
            existing_body_start = (existing_body_match.start()
                                   if existing_body_match else -1)
            new_body_start = (new_body_match.start() if new_body_match else -1)
            existing_body_end = existing_block.rfind("\n}")
            new_body_end = new_block.rfind("\n}")
            if min(existing_body_start, new_body_start, existing_body_end,
                   new_body_end) == -1:
                return new_block
            return (existing_block[:existing_body_start] +
                    new_block[new_body_start:new_body_end] +
                    existing_block[existing_body_end:])

        def shared_name_segments(test_name_a: str, test_name_b: str) -> int:
            shared_segments = 0
            for segment_a, segment_b in zip(test_name_a.split("_"),
                                            test_name_b.split("_")):
                if segment_a != segment_b:
                    break
                shared_segments += 1
            return shared_segments

        def find_generated_matches(test_file: str) -> List[re.Match[str]]:
            """Return regex matches for generated test blocks appearing
            after the '// Generated tests:' marker, or in the whole file
            if the marker is absent."""
            generated_tests_start = test_file.find("// Generated tests:")
            if generated_tests_start == -1:
                generated_tests_start = 0
            return [
                match for match in re.finditer(
                    'IN_PROC_BROWSER_TEST_[PF][\\(\\w\\s,]+'
                    f'{CoverageTest.TEST_ID_PREFIX}([a-zA-Z0-9._-]+)\\)'
                    '\\s*{\\n(?:\\s*\\/\\/.*\\n)+((?:[^;^}}]+;\\n)+)}',
                    test_file) if match.start() > generated_tests_start
            ]

        new_test_str = "\n\n".join(
            test.generate_browsertest(partition) for test in tests)
        if add_to_file:
            if os.path.exists(filename):
                with open(filename, "r", encoding="utf-8") as f:
                    test_file = f.read()
                generated_matches = find_generated_matches(test_file)
                existing_matches_by_name = {
                    match.group(1): match
                    for match in generated_matches
                }
                tests_by_name = {
                    test.generate_test_name(): test
                    for test in tests
                }

                if generated_matches:
                    # Phase 1: Update tests that already exist by name.
                    # Process in reverse file order so earlier match positions
                    # remain valid after each replacement.
                    replacement_names = [
                        test_name for test_name in tests_by_name
                        if test_name in existing_matches_by_name
                    ]
                    for test_name in sorted(
                            replacement_names,
                            key=lambda name: existing_matches_by_name[
                                name].start(),
                            reverse=True):
                        match = existing_matches_by_name[test_name]
                        new_block = tests_by_name[
                            test_name].generate_browsertest(partition)
                        updated_block = update_existing_generated_test_block(
                            match.group(0), new_block)
                        test_file = (test_file[:match.start()] +
                                     updated_block + test_file[match.end():])
                    if replacement_names:
                        generated_matches = find_generated_matches(test_file)
                        existing_matches_by_name = {
                            match.group(1): match
                            for match in generated_matches
                        }

                    # Phase 2: Insert new tests near the most similar existing
                    # test by name prefix.
                    insertion_names = [
                        test.generate_test_name() for test in ordered_tests
                        if test.generate_test_name() in tests_by_name
                        and test.generate_test_name() not in replacement_names
                    ]
                    for test_name in insertion_names:
                        insertion_position = (generated_matches[-1].end()
                                              if generated_matches else
                                              len(test_file))
                        max_shared_segments = 0
                        most_similar_name = None
                        for existing_name in existing_matches_by_name:
                            shared_segments = shared_name_segments(
                                existing_name, test_name)
                            if (shared_segments > max_shared_segments
                                    or (shared_segments == max_shared_segments
                                        and shared_segments > 0)):
                                max_shared_segments = shared_segments
                                most_similar_name = existing_name
                        if max_shared_segments >= 1:
                            insertion_position = existing_matches_by_name[
                                most_similar_name].end()
                        insertion_block = tests_by_name[
                            test_name].generate_browsertest(partition)
                        # Add spacing: prepend newlines when inserting after
                        # a test block; otherwise append newlines after insertion.
                        inserts_after_existing_test = (
                            (generated_matches and insertion_position
                             == generated_matches[-1].end()) or
                            (most_similar_name is not None
                             and insertion_position ==
                             existing_matches_by_name[most_similar_name].end())
                        )
                        if inserts_after_existing_test:
                            insertion_str = f"\n\n{insertion_block}"
                        else:
                            insertion_str = f"{insertion_block}\n\n"
                        test_file = (test_file[:insertion_position] +
                                     insertion_str +
                                     test_file[insertion_position:])
                        generated_matches = find_generated_matches(test_file)
                        existing_matches_by_name = {
                            match.group(1): match
                            for match in generated_matches
                        }

                # Fallback to adding the tests to the end of the file instead
                # of the smarter approach.
                if not generated_matches:
                    # Find the last test in the test file
                    matches = list(
                        re.finditer(r"IN_PROC_BROWSER_TEST_[PF](.|\n)*?}\n",
                                    test_file))
                    if matches:
                        last_test_end_index = matches[-1].end()
                    else:
                        # If no tests found, try to insert before the last closing brace
                        # (which is usually the closing namespace).
                        last_brace_index = test_file.rfind("}")
                        if last_brace_index != -1:
                            # Find the start of the line with the last brace to be clean.
                            last_test_end_index = test_file.rindex(
                                '\n', 0, last_brace_index) + 1
                        else:
                            last_test_end_index = len(test_file)
                    test_file = (test_file[:last_test_end_index] + "\n" +
                                 new_test_str + "\n" +
                                 test_file[last_test_end_index:])
                with open(filename, "w", encoding="utf-8") as f:
                    f.write(test_file)
            else:
                print(f"\n\nCreate a new test file: {filename}\n"
                      "Remember to add the new test file to the BUILD file.\n"
                      "Add the following tests to the new test file:\n"
                      f"\n{new_test_str}\n")
        else:
            print(f"\n\nAdd the following tests to {filename}:\n"
                  f"\n{new_test_str}\n")

    test_ids_to_keep: Dict[frozenset[TestPlatform],
                           Set[TestId]] = defaultdict(set)
    test_names_to_keep: Dict[frozenset[TestPlatform],
                             Set[str]] = defaultdict(set)
    for platforms, tests in required_tests.items():
        tests_to_add: List[CoverageTest] = []
        for test in tests:
            test_names_to_keep[platforms].add(test.generate_test_name())
            if platforms in existing_tests:
                existing_test_set = {
                    test_id
                    for (test_id, _) in existing_tests[platforms]
                }
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
            ordered_partition_tests = filter_tests_for_partition(
                tests, partition)
            write_tests(filename, tests_to_add_partition, partition,
                        ordered_partition_tests)

        # All remaining tests go into the default partition
        default_tests: List[CoverageTest] = [
            test for test in tests_to_add
            if test.id not in tests_added_to_partition
        ]
        if not default_tests:
            continue
        filename = default_partition.generate_browsertest_filepath(platforms)
        ordered_default_tests = [
            test for test in tests if test.id not in tests_added_to_partition
        ]
        write_tests(filename, default_tests, default_partition,
                    ordered_default_tests)
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
            tests_to_remove = [
                test_name for (_, test_name) in test_ids_names
                if test_name not in test_names_to_keep[platforms]
            ]
        else:
            prompt_str = (f"\n\nRemove these tests from the file for the "
                          f"platforms [{nice_platform_str}]:\n")
            tests_to_remove = [
                test_name for (test_id, test_name) in test_ids_names
                if test_id not in test_ids_to_keep[platforms]
                and test_name not in test_names_to_keep[platforms]
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
        queue = [test.actions]
        final_expanded_tests = []

        while queue:
            current_actions = queue.pop(0)
            expanded = get_all_parameterized_tests(current_actions)

            for expanded_test in expanded:
                if any(action.type is ActionType.PARAMETERIZED
                       for action in expanded_test):
                    queue.append(expanded_test)
                else:
                    final_expanded_tests.append(expanded_test)

        logging.info(
            f"Generated {len(final_expanded_tests)} test/s from {test.id}")
        for resulting_test in final_expanded_tests:
            result_tests.append(CoverageTest(resulting_test, test.platforms))
    return result_tests


def filter_coverage_tests_for_platform(tests: List[CoverageTest],
                                       platform: TestPlatform
                                       ) -> List[CoverageTest]:
    return [test for test in tests if platform in test.platforms]


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
