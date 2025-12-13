# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

import re

def CheckChange(input_api, output_api):
    """Checks that for every _test.ts file, there's a corresponding browser
    test defined in read_anything_browsertest.cc"""

    presubmit_dir = input_api.PresubmitLocalPath()

    # 1. Find all _test.ts files in the directory.
    ts_test_files_in_dir = set()
    for f in input_api.os_listdir(presubmit_dir):
        if f.endswith('_test.ts'):
            ts_test_files_in_dir.add(f)

    # 2. Read C++ browser test files from read_anything_browsertest.cc
    browser_test_path = input_api.os_path.join(
        presubmit_dir, 'read_anything_browsertest.cc')
    with open(browser_test_path, 'r', encoding='utf-8') as f:
        content = f.read()
    browser_test_js_files = set(
        re.findall(r'side_panel/read_anything/([^"]+\.js)', content))
    browser_test_files = {
        f.replace('.js', '.ts')
        for f in browser_test_js_files
    }

    # 3. Compare the lists
    missing_in_browser_tests = ts_test_files_in_dir - browser_test_files

    if missing_in_browser_tests:
        error_message = (
            'The following test files exist in the directory but are '
            'not in read_anything_browsertest.cc:\n')
        for f in sorted(list(missing_in_browser_tests)):
            error_message += f + '\n'
        return [output_api.PresubmitError(error_message)]

    return []
