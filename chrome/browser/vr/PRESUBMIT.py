# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting chrome/browser/vr

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


# chrome/PRESUBMIT.py blocks several linters due to the infeasibility of
# enforcing them on a large codebase. Here we'll start by enforcing all
# linters, and add exclusions if necessary.
LINT_FILTERS = [
  '-build/include',
]

VERBOSITY_LEVEL = 4

INCLUDE_CPP_FILES_ONLY = (r'.*\.(cc|h)$',)

def _CheckChangeLintsClean(input_api, output_api):
  sources = lambda x: input_api.FilterSourceFile(
      x, files_to_check=INCLUDE_CPP_FILES_ONLY)
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, sources, LINT_FILTERS, VERBOSITY_LEVEL)

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckChangeLintsClean(input_api, output_api))
  return results

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CheckChangeLintsClean(input_api, output_api))
  return results
