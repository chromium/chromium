# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for //components/reporting

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

from pathlib import Path

def CheckIncludeForFullPath(input_api, output_api):
  """Checks to make sure every .h file has a full path."""
  errors = []
  pattern = input_api.re.compile(r'^#include\s*\"(\S*)\"',
                                  input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if not f.LocalPath().casefold().endswith(('.h', '.cc')):
      continue
    contents = input_api.ReadFile(f)
    for include in pattern.finditer(contents):
      header = include.group(1)
      if not header.casefold().endswith('.h'):
        continue
      if not header.startswith('/'):
        header = '../../' + header
      if not Path(header).is_file():
        errors.append(f.LocalPath() + ': ' + include.group(1))
  if errors:
    return [ output_api.PresubmitError(
        'Some #include(s) in source errors do not have a full path. ',
        errors) ]
  return []
