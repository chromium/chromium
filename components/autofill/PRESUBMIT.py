# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/components/autofill.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

def IsComponentsAutofillFile(f, name_suffix):
  # The exact path can change. Only check the containing folder.
  return (f.LocalPath().startswith('components/autofill/') and
          f.LocalPath().endswith(name_suffix))

def AnyAffectedFileMatches(input_api, matcher):
  return any(matcher(f) for f in input_api.change.AffectedTestableFiles())

def IsComponentsAutofillFileAffected(input_api, name_suffix):
  return AnyAffectedFileMatches(
      input_api, lambda f: IsComponentsAutofillFile(f, name_suffix))

def _CheckNoBaseTimeCalls(input_api, output_api):
  """Checks that no files call base::Time::Now() or base::TimeTicks::Now()."""
  pattern = input_api.re.compile(
      r'(base::(Time|TimeTicks)::Now)\(\)',
      input_api.re.MULTILINE)
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('components/autofill/') and
        not f.LocalPath().endswith("PRESUBMIT.py")):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitPromptWarning(
        'Consider to not call base::Time::Now() or base::TimeTicks::Now() ' +
        'directly but use AutofillClock::Now() and '+
        'Autofill::TickClock::NowTicks(), respectively. These clocks can be ' +
        'manipulated through TestAutofillClock and TestAutofillTickClock '+
        'for testing purposes, and using AutofillClock and AutofillTickClock '+
        'throughout Autofill code makes sure Autofill tests refers to the '+
        'same (potentially manipulated) clock.',
        files) ]
  return []

def _CheckNoServerFieldTypeCasts(input_api, output_api):
  """Checks that no files cast (e.g., raw integers to) ServerFieldTypes."""
  pattern = input_api.re.compile(
      r'_cast<\s*ServerFieldType\b',
      input_api.re.MULTILINE)
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('components/autofill/') and
        not f.LocalPath().endswith("PRESUBMIT.py")):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitPromptWarning(
        'Do not cast raw integers to ServerFieldType to prevent values that ' +
        'have no corresponding enum constant or are deprecated. Use '+
        'ToSafeServerFieldType() instead.',
        files) ]
  return []

def _CheckFeatureNames(input_api, output_api):
  """Checks that no features are enabled."""

  pattern = input_api.re.compile(
          r'\bBASE_FEATURE\s*\(\s*k(\w*)\s*,\s*"(\w*)"',
          input_api.re.MULTILINE)
  warnings = []

  def exception(constant, feature):
    if constant == "AutofillAddressEnhancementVotes" and \
       feature == "kAutofillAddressEnhancementVotes":
      return True
    return False

  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if IsComponentsAutofillFile(f, 'features.cc'):
      contents = input_api.ReadFile(f)
      mismatches = [(constant, feature)
              for (constant, feature) in pattern.findall(contents)
              if constant != feature and not exception(constant, feature)]
      if mismatches:
        mismatch_strings = ['\t{} -- {}'.format(*m) for m in mismatches]
        mismatch_string = format('\n').join(mismatch_strings)
        warnings += [ output_api.PresubmitPromptWarning(
            'Feature names should be identical to variable names:\n{}'
                .format(mismatch_string),
            [f]) ]

  return warnings

def _CheckWebViewExposedExperiments(input_api, output_api):
  """Checks that changes to autofill features are exposed to webview."""

  _PRODUCTION_SUPPORT_FILE = ('android_webview/java/src/org/chromium/' +
      'android_webview/common/ProductionSupportedFlagList.java')

  warnings = []
  if (IsComponentsAutofillFileAffected(input_api, 'features.cc') and
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

def _CheckModificationOfLegacyRegexPatterns(input_api, output_api):
  """Reminds to update internal regex patterns when legacy ones are modified."""

  if IsComponentsAutofillFileAffected(input_api, "legacy_regex_patterns.json"):
    return [
        output_api.PresubmitPromptWarning(
            "You may need to modify the parsing patterns in src-internal. " +
            "See go/autofill-internal-parsing-patterns for more details. " +
            "Ideally, the legacy patterns should not be modified.")
    ]

  return []

def _CheckModificationOfFormAutofillUtil(input_api, output_api):
  """Reminds to keep form_autofill_util.cc and the iOS counterpart in sync."""

  if (IsComponentsAutofillFileAffected(input_api, "fill.js") !=
      IsComponentsAutofillFileAffected(input_api, "form_autofill_util.cc")):
    return [
        output_api.PresubmitPromptWarning(
            'Form extraction/label inference has a separate iOS ' +
            'implementation in components/autofill/ios/form_util/resources/' +
            'fill.js. Try to keep it in sync with form_autofill_util.cc.')
    ]

  return []

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckNoBaseTimeCalls(input_api, output_api))
  results.extend(_CheckNoServerFieldTypeCasts(input_api, output_api))
  results.extend(_CheckFeatureNames(input_api, output_api))
  results.extend(_CheckWebViewExposedExperiments(input_api, output_api))
  results.extend(_CheckModificationOfLegacyRegexPatterns(input_api, output_api))
  results.extend(_CheckModificationOfFormAutofillUtil(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
