# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for components/feature_engagement.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def _CommonChecks(input_api, output_api):
  results = []
  results.extend(_CheckFeatureConstantsSorting(input_api, output_api))
  return results

def _CheckFeatureConstantsSorting(input_api, output_api):
  FEATURE_CONSTANTS_PATH = (
      'components/feature_engagement/public/android/java/src/org/chromium/'
      'components/feature_engagement/FeatureConstants.java')

  affected_file = None
  for f in input_api.AffectedFiles():
    if f.LocalPath() == FEATURE_CONSTANTS_PATH:
      affected_file = f
      break

  if not affected_file:
    return []

  new_contents = affected_file.NewContents()
  results = []

  # 1. Check @StringDef block
  results.extend(_CheckKeepSortedBlock(input_api, output_api,
                 new_contents, FEATURE_CONSTANTS_PATH))

  # 2. Check String constants block
  results.extend(_CheckStringConstantsSorted(input_api, output_api,
                 new_contents, FEATURE_CONSTANTS_PATH))

  return results

def _CheckKeepSortedBlock(input_api, output_api, lines, file_path):
  in_block = False
  block_lines = []
  for line in lines:
    stripped = line.strip()
    if stripped and stripped.startswith('FeatureConstants.'):
      block_lines.append(stripped)

  if not block_lines:
    return []

  sorted_block = sorted(block_lines)
  if block_lines == sorted_block:
    return []

  # Find discrepancy
  for i in range(len(block_lines)):
    if block_lines[i] != sorted_block[i]:
      message = (
          f'The @StringDef block in {file_path} is not sorted alphabetically.\n'
          f'  - Actual item:   {block_lines[i]}\n'
          f'  - Expected item: {sorted_block[i]}'
      )
      return [output_api.PresubmitPromptWarning(message)]
  return []

def _CheckStringConstantsSorted(input_api, output_api, lines, file_path):
  string_constants = []
  for line in lines:
    # Match 'String NAME = '
    # Note: Using input_api.re for regex as is standard in Chromium presubmits.
    match = input_api.re.search(r'^\s*String\s+([A-Z0-9_]+)\s*=', line)
    if match:
      string_constants.append(match.group(1))

  if not string_constants:
    return []

  sorted_constants = sorted(string_constants)
  if string_constants == sorted_constants:
    return []

  # Find discrepancy
  for i in range(len(string_constants)):
    if string_constants[i] != sorted_constants[i]:
      message = (
          f'The String constants in {file_path} are not sorted alphabetically.'
          f'\n  - Actual item:   {string_constants[i]}\n'
          f'  - Expected item: {sorted_constants[i]}'
      )
      return [output_api.PresubmitError(message)]
  return []
