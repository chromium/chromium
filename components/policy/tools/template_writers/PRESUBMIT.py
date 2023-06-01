#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Template writers unittests presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

def RunUnittests(input_api, output_api):
  return input_api.canned_checks.RunPythonUnitTests(input_api,
                                                    output_api,
                                                    ['test_suite_all'],
                                                    python3=True)


def CheckChangeOnUpload(input_api, output_api):
  return RunUnittests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunUnittests(input_api, output_api)
