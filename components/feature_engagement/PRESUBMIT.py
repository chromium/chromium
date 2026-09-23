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
  results.extend(_CheckJavaConstantsSorting(input_api, output_api))
  results.extend(_CheckFeatureListSorting(input_api, output_api))
  results.extend(_CheckNoComparatorAny(input_api, output_api))
  results.extend(_CheckNoRedundantNamespaceQualifier(input_api, output_api))
  return results

def _CheckFeatureListSorting(input_api, output_api):
  FEATURE_LIST_H_PATH = 'components/feature_engagement/public/feature_list.h'
  FEATURE_LIST_CC_PATH = 'components/feature_engagement/public/feature_list.cc'

  results = []
  for f in input_api.AffectedFiles():
    if f.LocalPath() not in (FEATURE_LIST_H_PATH, FEATURE_LIST_CC_PATH):
      continue

    for line_num, line in f.ChangedContents():
      if '#if BUILDFLAG' in line:
        message = (
            f'It looks like you are adding a new BUILDFLAG block to '
            f'{f.LocalPath()}. Please re-use the existing blocks and keep '
            f'the items in alphabetical order within those blocks. If there '
            f'is no existing block that matches your new block, then you can '
            f'ignore this message. For questions or if this is too noisy, '
            f'please ping mschillaci@.'
        )
        results.append(output_api.PresubmitPromptWarning(message))
        break

  return results

def _CheckJavaConstantsSorting(input_api, output_api):
  FEATURE_CONSTANTS_PATH = (
      'components/feature_engagement/public/android/java/src/org/chromium/'
      'components/feature_engagement/FeatureConstants.java')
  EVENT_CONSTANTS_PATH = (
      'components/feature_engagement/public/android/java/src/org/chromium/'
      'components/feature_engagement/EventConstants.java')

  results = []
  for f in input_api.AffectedFiles():
    if f.LocalPath() == FEATURE_CONSTANTS_PATH:
      results.extend(_CheckFeatureConstantsFile(input_api, output_api, f))
    elif f.LocalPath() == EVENT_CONSTANTS_PATH:
      results.extend(_CheckEventConstantsFile(input_api, output_api, f))

  return results

def _CheckFeatureConstantsFile(input_api, output_api, affected_file):
  new_contents = affected_file.NewContents()
  file_path = affected_file.LocalPath()
  results = []

  # 1. Check @StringDef block
  results.extend(_CheckKeepSortedBlock(input_api, output_api,
                 new_contents, file_path))

  # 2. Check String constants block
  results.extend(_CheckStringConstantsSorted(input_api, output_api,
                 new_contents, file_path, r'^\s*String\s+([A-Z0-9_]+)\s*='))

  return results

def _CheckEventConstantsFile(input_api, output_api, affected_file):
  new_contents = affected_file.NewContents()
  file_path = affected_file.LocalPath()
  results = []

  # Check String constants block
  # Rules: if a line startsWith 'public static final String ',
  # then it gets added to our list
  results.extend(_CheckStringConstantsSorted(input_api, output_api,
                 new_contents, file_path,
                 r'^\s*public\s+static\s+final\s+String\s+([A-Z0-9_]+)\s*='))

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

def _CheckStringConstantsSorted(input_api, output_api, lines,
                                file_path, regex_pattern):
  string_constants = []
  for line in lines:
    # Match constant declaration based on regex_pattern
    # Note: Using input_api.re for regex as is standard in Chromium presubmits.
    match = input_api.re.search(regex_pattern, line)
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

def _CheckNoComparatorAny(input_api, output_api):
  """Checks that no code uses Comparator(ANY, ...) directly."""
  results = []
  pattern = input_api.re.compile(
      r'\b(?:feature_engagement::)?Comparator\s*\(\s*(?:feature_engagement::)?ANY\b')

  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()
    if not local_path.endswith(('.cc', '.h', '.mm', '.cpp')):
      continue

    # Skip unit tests that test Comparator/validator edge cases.
    if local_path.endswith(('_unittest.cc', '_unittest.h')):
      continue

    # Skip configuration.h where the constants are defined.
    if local_path.endswith(
        'components/feature_engagement/public/configuration.h'):
      continue

    for line_num, line in f.ChangedContents():
      if pattern.search(line):
        message = (
            f'{local_path}:{line_num}: Do not use Comparator(ANY, ...). '
            f'Use kAlwaysTrue (or kAlwaysAvailable / kNoRestrictions) instead.'
        )
        results.append(output_api.PresubmitError(message))

  return results

def _CheckNoRedundantNamespaceQualifier(input_api, output_api):
  """Checks that C++ files do not use redundant 'feature_engagement::' qualifiers."""
  results = []
  namespace_decl_pattern = input_api.re.compile(
      r'^\s*namespace\s+feature_engagement\b')

  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()
    if not local_path.endswith(('.cc', '.h', '.mm', '.cpp')):
      continue

    for line_num, line in f.ChangedContents():
      # Strip single-line comments
      stripped = line.split('//')[0].strip()
      # Skip comment-only lines in block comments
      if stripped.startswith('*') or stripped.startswith('/*'):
        continue
      # Skip namespace declarations (e.g. namespace feature_engagement::stats)
      if namespace_decl_pattern.match(stripped):
        continue

      if 'feature_engagement::' in stripped:
        message = (
            f'{local_path}:{line_num}: Redundant "feature_engagement::" qualifier. '
            f'Code in components/feature_engagement is already in the '
            f'feature_engagement namespace. If this is a false positive, please '
            f'ping mschillaci@.'
        )
        results.append(output_api.PresubmitPromptWarning(message))

  return results
