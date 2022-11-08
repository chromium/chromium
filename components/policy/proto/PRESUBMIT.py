# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True

import os
import sys


def RunOtherPresubmit(function_name, input_api, output_api):
  # Apply the PRESUBMIT for components/policy/resources to run the syntax check
  component_resources_path = os.path.join('components', 'policy', 'resources')

  # Skip the presubmit if //components/policy/resources is changed as well.
  if any(component_resources_path in os.path.dirname(changed_file.LocalPath())
         for changed_file in input_api.change.AffectedFiles()):
    return []

  # need to add components/policy/resources because PRESUBMIT.py there
  # imports policy_templates module in the directory.
  component_resources_path_absolute = os.path.join(
      input_api.change.RepositoryRoot(), component_resources_path)
  if (component_resources_path_absolute not in sys.path):
    sys.path.append(component_resources_path_absolute)

  presubmit_path = os.path.join(component_resources_path_absolute,
                                'PRESUBMIT.py')

  presubmit_content = input_api.ReadFile(presubmit_path)
  global_vars = {}
  exec(presubmit_content, global_vars)
  return global_vars[function_name](input_api, output_api)

def CheckDevicePolicyProtos(input_api, output_api):
  return RunOtherPresubmit("CheckDevicePolicyProtos", input_api, output_api)

