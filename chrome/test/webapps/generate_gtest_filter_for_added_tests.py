#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script used to generate the tests definitions for Web App testing framework.
See the README.md file in this directory for more information.

Usage: python3 chrome/test/webapps/generate_gtest_filter_for_added_tests.py
"""

import argparse
import subprocess
import logging
import os
import re
from typing import Dict, List, Set

DEFAULT_GIT_DIFF_ARGS = [
    '--no-ext-diff', '--unified=0', '--exit-code', '-a', '--no-prefix'
]

BROWSER_TESTS_FOLDER = 'chrome/browser/ui/views/web_apps/'
SYNC_INTEGRATION_TESTS_FOLDER = 'chrome/browser/sync/test/integration/'


def get_gtest_filter_list(diff_strategy: str) -> Dict[str, Set[str]]:
    script_folder = os.path.dirname(os.path.realpath(__file__))
    browser_tests_folder = os.path.join(script_folder, '..', '..', '..',
                                        BROWSER_TESTS_FOLDER)
    sync_integration_tests_folder = os.path.join(
        script_folder, '..', '..', '..', SYNC_INTEGRATION_TESTS_FOLDER)

    gtest_filters = {'browser_tests': set(), 'sync_integration_tests': set()}
    git_diff_cmd = ['git', 'diff']
    args = DEFAULT_GIT_DIFF_ARGS
    for folder in [browser_tests_folder, sync_integration_tests_folder]:
        if diff_strategy == 'staged':
            output_lines = _execute_cmd_from_src(git_diff_cmd + args +
                                                 ['--staged'] + [folder])
        elif diff_strategy == 'upstream':
            output_lines = _execute_cmd_from_src(git_diff_cmd + args +
                                                 ['@{upstream}'] + [folder])
        # Committed files is basically
        # upstream - staged files - unstaged files
        elif diff_strategy == 'committed':
            upstream_lines = _execute_cmd_from_src(git_diff_cmd + args +
                                                   ['@{upstream}'] + [folder])
            staged_lines = _execute_cmd_from_src(git_diff_cmd + args +
                                                 ['--staged'] + [folder])
            unstaged_lines = _execute_cmd_from_src(git_diff_cmd + args +
                                                   + [folder])
            output_lines = (list(
                set(upstream_lines) - set(staged_lines) - set(unstaged_lines)))
        elif diff_strategy == 'unstaged':
            output_lines = _execute_cmd_from_src(git_diff_cmd + args +
                                                 [folder])
        else:
            raise RuntimeError('diff_strategy "%s" is not supported.' %
                               diff_strategy)

        for line in output_lines:
            m = re.search(r'^\+.*(WAI\_[^)]+)\)', line.decode('utf-8'))
            if not m:
                continue
            test_name = str(m.group(1))
            if folder == browser_tests_folder:
                gtest_filters['browser_tests'].add(test_name)
            elif folder == sync_integration_tests_folder:
                gtest_filters['sync_integration_tests'].add(test_name)
    return gtest_filters


def print_gtest_filters(gtest_filters: Dict[str, Set[str]]):
    for test_binary in gtest_filters:
        if len(gtest_filters[test_binary]) == 0:
            continue
        print('Please run:')
        print('    %s --gtest_filter=WebAppIntegration.%s\n' %
              (test_binary, ':WebAppIntegration.'.join(
                  gtest_filters[test_binary])))


def _execute_cmd_from_src(cmd: List[str]) -> List[str]:
    working_dir = os.path.dirname(os.path.realpath(__file__))
    try:
        p = subprocess.Popen(cmd,
                             cwd=working_dir,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        return stdout.splitlines()
    except Exception as e:
        raise RuntimeError(
            'Error when running the cmd: %s.\n Error message: %s', cmd, str(e))


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--diff-strategy',
        dest='diff_strategy',
        help='Set the filter for looking up diff files.',
        required=False,
        choices=['upstream', 'committed', 'staged', 'unstaged'],
        default='upstream')

    options = parser.parse_args(argv)

    gtest_filters = get_gtest_filter_list(options.diff_strategy)
    print_gtest_filters(gtest_filters)


if __name__ == '__main__':
    main()
