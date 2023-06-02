# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for browser_sync component.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re

BROWSER_SYNC_SOURCE_FILES = (r'^components[\\/]browser_sync[\\/].*\.(cc|h)$',)

def CheckChangeLintsClean(input_api, output_api):
  source_filter = lambda x: input_api.FilterSourceFile(
    x, files_to_check=BROWSER_SYNC_SOURCE_FILES, files_to_skip=None)
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, source_filter, lint_filters=[], verbose_level=1)

def CheckChanges(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)
  return results

def CheckChangeOnUpload(input_api, output_api):
  return CheckChanges(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChanges(input_api, output_api)
