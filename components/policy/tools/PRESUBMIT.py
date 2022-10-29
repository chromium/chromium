# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

USE_PYTHON3 = True


def _RunOtherPresubmit(function_name, input_api, output_api):
  # Apply the PRESUBMIT for components/policy/resources to run the syntax check
  component_resources_path = os.path.join('components', 'policy', 'resources')

  # need to add components/policy/resources because PRESUBMIT.py there
  # imports policy_templates module in the directory.
  component_resources_path_absolute = os.path.join(
      input_api.change.RepositoryRoot(), component_resources_path)
  if (component_resources_path_absolute not in sys.path):
    sys.path.append(component_resources_path_absolute)

  # Skip the presubmit if //components/policy/resources is changed as well.
  if any(component_resources_path in os.path.dirname(changed_file.LocalPath())
         for changed_file in input_api.change.AffectedFiles()):
    return []

  presubmit_path = os.path.join(component_resources_path_absolute,
                                'PRESUBMIT.py')
  presubmit_content = input_api.ReadFile(presubmit_path)
  global_vars = {}
  exec(presubmit_content, global_vars)
  return global_vars[function_name](input_api, output_api)


def _RunPythonUnitTests(input_api, output_api):
  tests = input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      directory='.',
      files_to_check=[r'^.+_test\.py$'],
      run_on_python2=False)
  return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
  output = _RunOtherPresubmit("CheckChangeOnUpload", input_api, output_api)
  output.extend(_RunPythonUnitTests(input_api, output_api))
  return output


def CheckChangeOnCommit(input_api, output_api):
  output = _RunOtherPresubmit("CheckChangeOnCommit", input_api, output_api)
  output.extend(_RunPythonUnitTests(input_api, output_api))
  return output
