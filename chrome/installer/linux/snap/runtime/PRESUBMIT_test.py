#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import unittest

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, file_dir_path)
import PRESUBMIT  # noqa: E402

repo_root = os.path.abspath(
    os.path.join(file_dir_path, '..', '..', '..', '..', '..')
)
sys.path.insert(0, repo_root)
from PRESUBMIT_test_mocks import MockAffectedFile  # noqa: E402
from PRESUBMIT_test_mocks import MockInputApi  # noqa: E402
from PRESUBMIT_test_mocks import MockOutputApi  # noqa: E402

_REL_DIR = os.path.join('chrome', 'installer', 'linux', 'snap', 'runtime')


def _make_input_api(files, no_diffs=False):
    input_api = MockInputApi()
    input_api.presubmit_local_path = file_dir_path
    input_api.no_diffs = no_diffs
    input_api.files = files
    return input_api


class CheckVersionBumpedTest(unittest.TestCase):
    def testNoDiffsSkipsVersionCheck(self):
        # CI linux-presubmit and win-presubmit run with --all --no_diffs,
        # passing every tracked file with OldContents() == NewContents().
        input_api = _make_input_api(
            [
                MockAffectedFile(
                    os.path.join(_REL_DIR, '3pp', 'build.py'),
                    ['# build'],
                    old_contents=['# build'],
                    action='M',
                ),
                MockAffectedFile(
                    os.path.join(_REL_DIR, '3pp', 'runtime.lock.json'),
                    ['{"version": "20261103.3"}'],
                    old_contents=['{"version": "20261103.3"}'],
                    action='M',
                ),
            ],
            no_diffs=True,
        )
        errors = PRESUBMIT.CheckVersionBumped(input_api, MockOutputApi())
        self.assertEqual([], errors)

    def testModifiedWithoutVersionBumpFails(self):
        input_api = _make_input_api(
            [
                MockAffectedFile(
                    os.path.join(_REL_DIR, '3pp', 'build.py'),
                    ['# updated'],
                    old_contents=['# old'],
                    action='M',
                ),
                MockAffectedFile(
                    os.path.join(_REL_DIR, '3pp', 'runtime.lock.json'),
                    ['{"version": "20261103.3"}'],
                    old_contents=['{"version": "20261103.3"}'],
                    action='M',
                ),
            ]
        )
        errors = PRESUBMIT.CheckVersionBumped(input_api, MockOutputApi())
        self.assertEqual(1, len(errors))
        self.assertIn('need a new "version"', errors[0].message)

    def testModifiedWithVersionBumpPasses(self):
        input_api = _make_input_api(
            [
                MockAffectedFile(
                    os.path.join(_REL_DIR, '3pp', 'build.py'),
                    ['# updated'],
                    old_contents=['# old'],
                    action='M',
                ),
                MockAffectedFile(
                    os.path.join(_REL_DIR, '3pp', 'runtime.lock.json'),
                    ['{"version": "20261103.4"}'],
                    old_contents=['{"version": "20261103.3"}'],
                    action='M',
                ),
            ]
        )
        errors = PRESUBMIT.CheckVersionBumped(input_api, MockOutputApi())
        self.assertEqual([], errors)

    def testUnittestOnlyChangePasses(self):
        input_api = _make_input_api(
            [
                MockAffectedFile(
                    os.path.join(_REL_DIR, '3pp', 'build_unittest.py'),
                    ['# test'],
                    old_contents=['# old test'],
                    action='M',
                ),
            ]
        )
        errors = PRESUBMIT.CheckVersionBumped(input_api, MockOutputApi())
        self.assertEqual([], errors)


if __name__ == '__main__':
    unittest.main()
