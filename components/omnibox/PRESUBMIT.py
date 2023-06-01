# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting components/omnibox/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

def _CheckPedalsHistograms(input_api, output_api):
  """Check that no `OmniboxPedalId` values were added without also
  changing enums.xml"""
  results = []
  header = next((f for f in input_api.AffectedFiles()
                 if os.path.basename(f.LocalPath()) ==
                 "omnibox_pedal_concepts.h"), None)
  if header != None:
    def count_id_enum_lines(contents):
      indices = list(index for index, element in enumerate(contents)
                     if "NONE = 0" in element or "TOTAL_COUNT" in element)
      if len(indices) == 2:
        return indices[1] - indices[0]
      else:
        return 0
    new_lines = count_id_enum_lines(header.NewContents())
    old_lines = count_id_enum_lines(header.OldContents())
    if old_lines > new_lines:
      results.append(output_api.PresubmitPromptWarning(
          ("OmniboxPedalId enum has lines removed. Ensure that no enum "
           "values were removed since the ID list is append-only.")))
    elif new_lines > old_lines:
      # User seems to have added lines within the enum.
      enums = next((x for x in input_api.change.AffectedFiles()
                    if x.LocalPath().replace(os.sep, '/') ==
                    "tools/metrics/histograms/enums.xml"), None)
      if enums == None:
        results.append(output_api.PresubmitPromptWarning(
            ("OmniboxPedalId enum changed but "
             "tools/metrics/histograms/enums.xml did not. Check that each "
             "ID has a corresponding entry in the SuggestionPedalType enum.")))

  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckPedalsHistograms(input_api, output_api))
  return results

