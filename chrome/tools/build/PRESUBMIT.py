# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for FILES.cfg controlling which files are archived.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

_PLATFORMS = ['win']

def _CheckChange(input_api, output_api):
  results = []
  affected_files = input_api.change.LocalPaths()

  for platform in _PLATFORMS:
    files_config_path = input_api.os_path.join(platform, 'FILES.cfg')
    for filepath in affected_files:
      if filepath.endswith(files_config_path):
        output, error = input_api.subprocess.Popen(
            ['python3', files_config_path],
            stdout=input_api.subprocess.PIPE,
            stderr=input_api.subprocess.PIPE).communicate()
        if output or error:
          results.append(output_api.PresubmitError(
              files_config_path + " syntax error: \n" + bytes.decode(output) +
              bytes.decode(error)))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckChange(input_api, output_api))
  return results

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CheckChange(input_api, output_api))
  return results
