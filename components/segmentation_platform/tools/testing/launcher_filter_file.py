#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script provides functionality for automatically keeping a launcher
filter file up to date for tests in a particular component.

By running this script as a binary, it automatically rewrites the filter file,
but it can also be used as a Python module for presubmit scripts.

The resulting file can be used through the test binary, e.g.:
./out/Default/components_unittests \
  --test-launcher-filter-file=components/my_feature/components_unittests.filter
"""

from typing import List

import os
import re

OUTPUT_FILENAME = 'components_unittests.filter'


def GetComponentDirectoryPath() -> str:
    # Returns the path to the current component.
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')


def GetLauncherFilterFilePath(filename) -> str:
    # Returns the path to the filter file passed to the test binary.
    component_directory = GetComponentDirectoryPath()
    return os.path.join(component_directory, filename)


def FindTestSuites(cwd: str) -> List[str]:
    # Finds all test suites that are in the current component directory.
    #
    # Search for all test suites that match one of these patterns:
    # *   TEST_F(MyTest, ...)
    # *   TEST_P(MyTest, ...)
    # *   TEST(MyTest, ...)
    # All relevant matches will be in a named capture group called 'suite'.
    test_search = re.compile(r'^TEST(_F|_P)?\s*\(\s*(?P<suite>\s*[^,]+)',
                             re.MULTILINE)
    # Use a set to ensure we only get unique test suites.
    test_suites = set()
    # Walk through all directories to find all *.cc files.
    for root, _, files in os.walk(cwd):
        for filename in files:
            bare_filename, extension = os.path.splitext(filename)
            if extension == '.cc' and bare_filename.endswith('test'):
                file_path = os.path.join(root, filename)
                with open(file_path, 'r', encoding='utf-8') as f:
                    file_contents = f.read()
                    # Find all the group matches in the regex.
                    for matches in [
                            m.groupdict()
                            for m in test_search.finditer(file_contents)
                    ]:
                        # Only keep matches that are named 'suite'.
                        if 'suite' in matches:
                            test_suites.add(matches['suite'])

    return test_suites


def CreateLauncherFilterFileContent(test_suites: List[str]) -> str:
    # Uses the test suite names to create the string that can be stored as the
    # test launcher filter file.
    file_lines = ['*' + test_suite + '.*' for test_suite in test_suites]
    sorted_lines = sorted(file_lines)
    return '\n'.join(sorted_lines) + '\n'


def GetActualLauncherFilterFileContent() -> str:
    # Reads the current content of the default launcher filter file into a
    # string.
    file_path = GetLauncherFilterFilePath(OUTPUT_FILENAME)
    with open(file_path, 'r', encoding='utf-8') as f:
        return f.read()


def GetExpectedLauncherFilterFileContent() -> str:
    # Creates the content of the expected launcher filter file, with all known
    # test suites included.
    component_directory = GetComponentDirectoryPath()

    test_suites = FindTestSuites(component_directory)
    return CreateLauncherFilterFileContent(test_suites)


def WriteLauncherFilterFile(file_path: str, filter_file_content: str) -> None:
    # Writes out the filter file content to the given path.
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(filter_file_content)


def main():
    output_file_path = GetLauncherFilterFilePath(OUTPUT_FILENAME)
    expected_filter_file_content = GetExpectedLauncherFilterFileContent()
    WriteLauncherFilterFile(output_file_path, expected_filter_file_content)


if __name__ == '__main__':
    main()
