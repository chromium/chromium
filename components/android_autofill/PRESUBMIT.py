# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/components/android_autofill.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

def IsComponentsAndroidAutofillFile(f, name_suffix):
  return (f.LocalPath().startswith('components/android_autofill/') and
          f.LocalPath().endswith(name_suffix))

def AnyAffectedFileMatches(input_api, matcher):
  return any(matcher(f) for f in input_api.change.AffectedTestableFiles())

def IsComponentsAndroidAutofillFileAffected(input_api, name_suffix):
  return AnyAffectedFileMatches(
      input_api, lambda f: IsComponentsAndroidAutofillFile(f, name_suffix))

def CheckWebViewExposedExperiments(input_api, output_api):
  """Checks that changes to android autofill features are exposed to webview."""

  _PRODUCTION_SUPPORT_FILE = ('android_webview/java/src/org/chromium/' +
      'android_webview/common/ProductionSupportedFlagList.java')

  warnings = []
  if (IsComponentsAndroidAutofillFileAffected(input_api, 'features.cc') and
      not AnyAffectedFileMatches(
          input_api, lambda f: f.LocalPath() == _PRODUCTION_SUPPORT_FILE)):
    warnings += [
        output_api.PresubmitPromptWarning(
            (
                'You may need to modify {} instructions if your feature affects'
                ' WebView.'
            ).format(_PRODUCTION_SUPPORT_FILE)
        )
    ]

  return warnings
