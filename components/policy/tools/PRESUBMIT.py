# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

USE_PYTHON3 = True

def _RunPythonUnitTests(input_api, output_api):
  tests = input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      directory='.',
      files_to_check=[r'^.+_test\.py$'],
      run_on_python2=False)
  return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
  return _RunPythonUnitTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _RunPythonUnitTests(input_api, output_api)
