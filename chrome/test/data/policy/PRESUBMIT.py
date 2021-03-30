# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

def RunOtherPresubmit(function_name, input_api, output_api):
  # Apply the PRESUBMIT for components/policy/resources to run the syntax check
  component_resources_path = os.path.join('components', 'policy', 'resources')

  # Skip the presubmit if //components/policy/resources is changed as well.
  if any(component_resources_path in os.path.dirname(changed_file.LocalPath())
         for changed_file in input_api.change.AffectedFiles()):
    return []

  presubmit_path = os.path.join(input_api.change.RepositoryRoot(),
                                component_resources_path, 'PRESUBMIT.py')

  presubmit_content = input_api.ReadFile(presubmit_path)
  global_vars = {}
  exec(presubmit_content, global_vars)
  return global_vars[function_name](input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
  return RunOtherPresubmit("CheckChangeOnUpload", input_api, output_api)



def CheckChangeOnCommit(input_api, output_api):
  return RunOtherPresubmit("CheckChangeOnCommit", input_api, output_api)
