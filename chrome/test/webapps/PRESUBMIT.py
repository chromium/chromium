# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for /chrome/test/webapps.

Runs Python unit tests in /chrome/test/webapps on upload.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckPythonUnittestsPass(input_api, output_api):
    results = []
    this_dir = input_api.PresubmitLocalPath()

    results += input_api.RunTests(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            this_dir,
            files_to_check=['.*unittest.py$'],
            env=None))

    return results
