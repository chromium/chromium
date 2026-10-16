#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import tempfile
import unittest
from unittest import mock

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.normpath(os.path.join(_THIS_DIR, '..', '..'))
# PRESUBMIT.py extends sys.path relative to the current directory, which
# presubmit_support.py sets to components/policy. Add the same paths here so
# that the tests can run from any directory.
sys.path.extend(
    [
        _SRC_DIR,
        os.path.join(_THIS_DIR, 'resources'),
        os.path.join(_SRC_DIR, 'third_party'),
        os.path.join(_SRC_DIR, 'third_party', 'pyjson5', 'src'),
    ]
)

from components.policy import PRESUBMIT  # noqa: E402
from PRESUBMIT_test_mocks import (  # noqa: E402
    MockAffectedFile,
    MockChange,
    MockInputApi,
    MockOutputApi,
)

_DEFINITION_PATH = os.path.join(
    'components',
    'policy',
    'resources',
    'templates',
    'policy_definitions',
    'Miscellaneous',
    'Foo.yaml',
)
_TEST_CASES_PATH = os.path.join(
    'components', 'policy', 'test', 'data', 'pref_mapping', 'Foo.json'
)
_IOS_TEST_CASES_PATH = os.path.join(
    'ios', 'chrome', 'test', 'data', 'policy', 'pref_mapping', 'Foo.json'
)


def _TestCase(os_list):
    return {
        'os': os_list,
        'simple_policy_pref_mapping_test': {'pref_name': 'foo'},
    }


class CheckPolicyTestCasePlatformsTest(unittest.TestCase):
    def setUp(self):
        temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(temp_dir.cleanup)
        self.root = temp_dir.name

    def _WriteTestCases(self, path, test_cases):
        full_path = os.path.join(self.root, path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, 'w', encoding='utf-8') as f:
            if isinstance(test_cases, str):
                f.write(test_cases)
            else:
                json.dump(test_cases, f)

    def _RunCheck(
        self, supported_on, future_on=(), modified_path=_DEFINITION_PATH
    ):
        '''Runs the check for a change modifying `modified_path`, with policy
        Foo supported on `supported_on` and `future_on` at version 150.'''
        input_api = MockInputApi()
        input_api.change = MockChange([MockAffectedFile(modified_path, [])])
        input_api.change.RepositoryRoot = lambda: self.root
        changed_policies = (
            [{'policy': 'Foo'}] if modified_path == _DEFINITION_PATH else []
        )
        policies = {
            'Foo': {'supported_on': supported_on, 'future_on': list(future_on)}
        }
        with (
            mock.patch.object(
                PRESUBMIT, '_GetCurrentVersion', return_value=150
            ),
            mock.patch.object(
                PRESUBMIT,
                '_GetPolicyChangeList',
                return_value=changed_policies,
            ),
            mock.patch.object(
                PRESUBMIT, '_GetPolicyDefinitionMap', return_value=policies
            ),
        ):
            results = PRESUBMIT.CheckPolicyTestCasePlatforms(
                input_api, MockOutputApi()
            )
        return [result.message for result in results]

    def testAllSupportedPlatformsTested(self):
        self._WriteTestCases(
            _TEST_CASES_PATH, [_TestCase(['win', 'mac', 'linux', 'chromeos'])]
        )
        self.assertEqual(
            self._RunCheck(['chrome.*:100-', 'chrome_os:100-']), []
        )

    def testMissingPlatforms(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['win'])])
        messages = self._RunCheck(['chrome.*:100-', 'chrome_os:100-'])
        self.assertEqual(len(messages), 1)
        self.assertIn(
            f'{_TEST_CASES_PATH} has no pref mapping test case or '
            'reason_for_missing_test for chromeos, linux, mac,',
            messages[0],
        )

    def testReasonWithoutOsCoversAllPlatforms(self):
        self._WriteTestCases(
            _TEST_CASES_PATH, [{'reason_for_missing_test': 'Reason'}]
        )
        self.assertEqual(self._RunCheck(['chrome.*:100-', 'android:100-']), [])

    def testReasonWithOsCoversItsPlatforms(self):
        self._WriteTestCases(
            _TEST_CASES_PATH,
            [
                _TestCase(['win', 'mac', 'linux']),
                {'os': ['android'], 'reason_for_missing_test': 'Reason'},
            ],
        )
        self.assertEqual(self._RunCheck(['chrome.*:100-', 'android:100-']), [])

    def testDesktopAndroidCoversAndroid(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['desktop_android'])])
        self.assertEqual(self._RunCheck(['android:100-']), [])

    def testWebviewAndroidRequiresAndroid(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['win'])])
        messages = self._RunCheck(['webview_android:100-'])
        self.assertEqual(len(messages), 1)
        self.assertIn('reason_for_missing_test for android,', messages[0])

        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['android'])])
        self.assertEqual(self._RunCheck(['webview_android:100-']), [])

    def testChromeWin7RequiresWin(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['mac'])])
        messages = self._RunCheck(['chrome.win7:100-'])
        self.assertEqual(len(messages), 1)
        self.assertIn('reason_for_missing_test for win,', messages[0])

        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['win'])])
        self.assertEqual(self._RunCheck(['chrome.win7:100-']), [])

    def testEmptyOsListWithoutReasonCoversNothing(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase([])])
        messages = self._RunCheck(['chrome.win:100-'])
        self.assertEqual(len(messages), 1)
        self.assertIn('reason_for_missing_test for win,', messages[0])

    def testFutureOnPlatformsAreRequired(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['chromeos', 'win'])])
        messages = self._RunCheck(['chrome_os:100-'], future_on=['chrome.*'])
        self.assertEqual(len(messages), 1)
        self.assertIn('reason_for_missing_test for linux, mac,', messages[0])

        self._WriteTestCases(
            _TEST_CASES_PATH, [_TestCase(['chromeos', 'win', 'mac', 'linux'])]
        )
        self.assertEqual(
            self._RunCheck(['chrome_os:100-'], future_on=['chrome.*']), []
        )

    def testFuchsiaIsIgnored(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['win'])])
        self.assertEqual(
            self._RunCheck(
                ['chrome.win:100-', 'fuchsia:100-'], future_on=['fuchsia']
            ),
            [],
        )

    def testIosOnlyUsesIosTestCases(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['win', 'ios'])])
        messages = self._RunCheck(['chrome.win:100-', 'ios:100-'])
        self.assertEqual(len(messages), 1)
        self.assertIn(f'{_IOS_TEST_CASES_PATH} has no', messages[0])

        self._WriteTestCases(_IOS_TEST_CASES_PATH, [_TestCase(['ios'])])
        self.assertEqual(self._RunCheck(['chrome.win:100-', 'ios:100-']), [])

    def testPlatformNoLongerSupported(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['mac'])])
        self.assertEqual(
            self._RunCheck(['chrome.win:100-149', 'chrome.mac:100-']), []
        )

    def testModifiedTestCasesAreChecked(self):
        self._WriteTestCases(_TEST_CASES_PATH, [_TestCase(['win'])])
        messages = self._RunCheck(
            ['chrome.mac:100-'], modified_path=_TEST_CASES_PATH
        )
        self.assertEqual(len(messages), 1)
        self.assertIn("for mac, which policy 'Foo'", messages[0])

    def testModifiedIosTestCasesAreChecked(self):
        self._WriteTestCases(_IOS_TEST_CASES_PATH, [_TestCase(['win'])])
        messages = self._RunCheck(
            ['ios:100-'], modified_path=_IOS_TEST_CASES_PATH
        )
        self.assertEqual(len(messages), 1)
        self.assertIn(f'{_IOS_TEST_CASES_PATH} has no', messages[0])

    def testUnrelatedChangeIsSkipped(self):
        self.assertEqual(
            self._RunCheck(
                ['chrome.mac:100-'],
                modified_path=os.path.join('components', 'policy', 'BUILD.gn'),
            ),
            [],
        )

    def testCommentsInTestCases(self):
        self._WriteTestCases(
            _TEST_CASES_PATH,
            '[\n'
            '  // Comments are allowed.\n'
            '  {"os": ["win"], "reason_for_missing_test": "Reason"}\n'
            ']\n',
        )
        self.assertEqual(self._RunCheck(['chrome.win:100-']), [])

    def testMalformedTestCases(self):
        self._WriteTestCases(_TEST_CASES_PATH, '[{"os": ["win"],')
        messages = self._RunCheck(['chrome.win:100-'])
        self.assertEqual(len(messages), 1)
        self.assertIn(f'Failed to parse {_TEST_CASES_PATH}', messages[0])


if __name__ == '__main__':
    unittest.main()
