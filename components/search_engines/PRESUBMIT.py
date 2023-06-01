# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting components/search_engines/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

def _CheckPrepopulatedEnginesVersion(input_api, output_api):
  """Check that no changes were made to prepopulated_engines.json without
  also updating kCurrentDataVersion"""
  results = []
  file = next((f for f in input_api.AffectedFiles()
                 if os.path.basename(f.LocalPath()) ==
                 "prepopulated_engines.json"), None)

  if file != None and not any(line for line in file.ChangedContents()
                              if "kCurrentDataVersion" in line[1]):
      results.append(output_api.PresubmitPromptWarning(
          ("prepopulated_engines.json changed but kCurrentDataVersion "
           "did not. Please ensure the version is rolled up when making "
           "meaningful changes to prepopulated_engines.json")))

  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckPrepopulatedEnginesVersion(input_api, output_api))
  return results

