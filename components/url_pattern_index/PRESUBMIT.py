# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for components/url_pattern_index directory.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def CheckUrlPatternIndexFormatVersion(input_api, output_api):
  """ Checks the kUrlPatternIndexFormatVersion is modified when necessary.

  Whenever any of the following files is changed:
    - components/url_pattern_index/flat/url_pattern_index.fbs
    - components/url_pattern_index/url_pattern_index.cc
  and kUrlPatternIndexFormatVersion stays intact, this check returns a
  presubmit warning to make sure the value is updated if necessary.
  """

  url_pattern_index_files_changed = False
  url_pattern_index_version_changed = False

  for affected_file in input_api.AffectedFiles():
    basename = input_api.basename(affected_file.LocalPath())

    if (basename == 'url_pattern_index.fbs' or
        basename == 'url_pattern_index.cc'):
      url_pattern_index_files_changed = True

    if basename == 'url_pattern_index.h':
      for (_, line) in affected_file.ChangedContents():
        if 'constexpr int kUrlPatternIndexFormatVersion' in line:
          url_pattern_index_version_changed = True
          break

  out = []
  if url_pattern_index_files_changed and not url_pattern_index_version_changed:
    out.append(output_api.PresubmitPromptWarning(
        'Please make sure that url_pattern_index::kUrlPatternIndexFormatVersion'
        ' is modified if necessary.'))

  return out

def CheckChangeOnUpload(input_api, output_api):
  return CheckUrlPatternIndexFormatVersion(input_api, output_api)
