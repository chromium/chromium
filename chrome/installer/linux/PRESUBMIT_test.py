#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import shutil
import sys
import unittest
from unittest import mock

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_dir_path, "..", "..", ".."))
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

sys.path.insert(0, os.path.join(file_dir_path, "common"))
import installer


@unittest.skipIf(not shutil.which("fakeroot") or not shutil.which("dpkg-deb"),
                 "fakeroot or dpkg-deb not available")
class CheckRepoPackageVersionBumpTest(unittest.TestCase):

    def testRepoPackageHashMatches(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = []
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckRepoPackageVersionBump(mock_input_api,
                                                        mock_output_api)
        self.assertEqual(0, len(errors))

    def testRepoPackageHashMismatch(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                "chrome/installer/linux/debian/repo_package.include",
                [
                    "REPO_PACKAGE_VERSION=3",
                    "REPO_PACKAGE_TIMESTAMP=1776966277",
                    "REPO_PACKAGE_HASH=deadbeef",
                ],
            )
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckRepoPackageVersionBump(mock_input_api,
                                                        mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertIn("does not match REPO_PACKAGE_HASH in", errors[0].message)

    def testMissingRepoPackageHash(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile(
                "chrome/installer/linux/debian/repo_package.include",
                [
                    "REPO_PACKAGE_VERSION=3",
                    "REPO_PACKAGE_TIMESTAMP=1776966277",
                ],
            )
        ]
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckRepoPackageVersionBump(mock_input_api,
                                                        mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertIn("REPO_PACKAGE_HASH not found in", errors[0].message)

    @mock.patch.object(
        installer,
        "compute_repo_package_hash_for_presubmit",
        side_effect=RuntimeError("mocked failure"))
    def testBuildDebFailure(self, _mock_build):
        mock_input_api = MockInputApi()
        mock_input_api.files = []
        mock_output_api = MockOutputApi()
        errors = PRESUBMIT._CheckRepoPackageVersionBump(mock_input_api,
                                                        mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertIn("Failed to build repo package for presubmit check",
                      errors[0].message)


if __name__ == "__main__":
    unittest.main()
