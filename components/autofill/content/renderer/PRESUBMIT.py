# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/components/autofill/content/renderer.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

def _CheckNoDirectPasswordCalls(input_api, output_api):
  """Checks that no files call IsPasswordField() or FormControlType()."""
  pattern = input_api.re.compile(
      r'(IsPasswordField|FormControlType)\(\)',
      input_api.re.MULTILINE)
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('components/autofill/content/renderer/') and
        not f.LocalPath().endswith("PRESUBMIT.py")):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitPromptWarning(
        'Consider to not call IsPasswordField() or FormControlType() directly ' +
        'but use IsPasswordFieldForAutofill() and FormControlTypeForAutofill() ' +
        'respectively. These declare text input fields as password fields ' +
        'if they have been password fields in the past. This is relevant ' +
        'for websites that allow to reveal passwords with a button that ' +
        'triggers a change of the type attribute of an <input> element.',
        files) ]
  return []


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckNoDirectPasswordCalls(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
