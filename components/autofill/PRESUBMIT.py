# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/components/autofill.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

import filecmp
import os
import re
import subprocess

def IsComponentsAutofillFile(f, name_suffix):
  # The exact path can change. Only check the containing folder.
  return (f.LocalPath().startswith('components/autofill/') and
          f.LocalPath().endswith(name_suffix))

def AnyAffectedFileMatches(input_api, matcher):
  return any(matcher(f) for f in input_api.change.AffectedTestableFiles())

def IsComponentsAutofillFileAffected(input_api, name_suffix):
  return AnyAffectedFileMatches(
      input_api, lambda f: IsComponentsAutofillFile(f, name_suffix))

def CheckNoAutofillClockTimeCalls(input_api, output_api):
  """Checks that no files call AutofillClock::Now()."""
  pattern = input_api.re.compile(r'(AutofillClock::Now)\(\)')
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('components/autofill/') and
        not f.LocalPath().endswith("PRESUBMIT.py")):
      if any(pattern.search(line) for _, line in f.ChangedContents()):
          files.append(f)

  if len(files):
    return [ output_api.PresubmitPromptWarning(
        'Consider to not call AutofillClock::Now() but use ' +
        'base::Time::Now(). AutofillClock will be deprecated and deleted soon.',
        files) ]
  return []

def CheckNoFieldTypeCasts(input_api, output_api):
  """Makes sure that raw integers aren't cast to FieldTypes."""
  explanation = """
Do not cast raw integers to FieldType to prevent values that
have no corresponding enum constant or are deprecated. Use
ToSafeFieldType() instead.
Add "// nocheck" to the end of the line to suppress this error."""
  errors = []
  file_filter = lambda f: (
    f.LocalPath().startswith('components/autofill/')
    and f.LocalPath().endswith(('.h', '.cc', '.mm'))
  )
  # There may be a line break in the cast, so we test multiple patterns.
  pattern_full = input_api.re.compile(r'_cast<\s*FieldType\b')
  pattern_prefix = input_api.re.compile(r'_cast<\s*$')
  pattern_postfix = input_api.re.compile(r'^\s*FieldType\b')
  for f in input_api.AffectedSourceFiles(file_filter):
    contents = f.ChangedContents()
    # We look at each line and their successor to check if
    # - the line contains the full `static_cast<FieldType>` or similar, or
    # - the line ends with `static_cast<` and the next line begins with
    #   `FieldType` or similar.
    for i in range(len(contents)):
      line_num = contents[i][0]
      line = contents[i][1]
      next_line = contents[i+1][1] if i+1 < len(contents) else ''
      if line.endswith("// nocheck"):
        continue
      if next_line.endswith("// nocheck"):
        next_line = ''
      line = line.split('//')[0]
      next_line = next_line.split('//')[0]
      if pattern_full.search(line) or (
          pattern_prefix.search(line) and pattern_postfix.search(next_line)
      ):
        errors.append(
            output_api.PresubmitError(
                f'{f.LocalPath()}:{line_num}: {explanation}'
            )
        )
  return errors

def CheckFieldTypeSets(input_api, output_api):
  """Produces errors if the changed code contains DenseSet<FieldType> instead
of FieldType, and similarly for FieldTypeGroupSet and HtmlFieldTypeSet."""
  bad_patterns = [
      (
          input_api.re.compile(r'\bDenseSet<FieldType>'),
          'Use FieldTypeSet instead of DenseSet<FieldType>',
      ),
      (
          input_api.re.compile(r'\bDenseSet<FieldTypeGroup>'),
          'Use FieldTypeGroupSet instead of DenseSet<FieldTypeGroup>',
      ),
      (
          input_api.re.compile(r'\bDenseSet<HtmlFieldType>'),
          'Use HtmlFieldTypeSet instead of DenseSet<HtmlFieldType>',
      ),
  ]
  warnings = []
  file_filter = lambda f: (
    f.LocalPath().startswith('components/autofill/')
    and f.LocalPath().endswith(('.h', '.cc', '.mm'))
  )
  for file in input_api.AffectedSourceFiles(file_filter):
    for line_num, line in file.ChangedContents():
      if line.endswith("// nocheck"):
        continue
      line = line.split('//')[0]
      for regex, explanation in bad_patterns:
        if regex.search(line):
          warnings.append(
              output_api.PresubmitError(
                  f'{file.LocalPath()}:{line_num}: {explanation}. Add '
                  '"// nocheck" to the end of the line to suppress this error.'
              )
          )
  return warnings

def CheckFeatureNames(input_api, output_api):
  """Checks that no features are enabled."""

  pattern = input_api.re.compile(
          r'\bBASE_FEATURE\s*\(\s*k(\w*)\s*,\s*"(\w*)"',
          input_api.re.MULTILINE)
  warnings = []

  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if IsComponentsAutofillFile(f, 'features.cc'):
      contents = input_api.ReadFile(f)
      mismatches = [(constant, feature)
              for (constant, feature) in pattern.findall(contents)
              if constant != feature]
      if mismatches:
        mismatch_strings = ['\t{} -- {}'.format(*m) for m in mismatches]
        mismatch_string = format('\n').join(mismatch_strings)
        warnings += [ output_api.PresubmitPromptWarning(
            'Feature names should be identical to variable names:\n{}'
                .format(mismatch_string),
            [f]) ]

  return warnings

def CheckWebViewExposedExperiments(input_api, output_api):
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

def CheckModificationOfLegacyRegexPatterns(input_api, output_api):
  """Reminds to update internal regex patterns when legacy ones are modified."""

  if IsComponentsAutofillFileAffected(input_api, "legacy_regex_patterns.json"):
    return [
        output_api.PresubmitPromptWarning(
            "You may need to modify the parsing patterns in src-internal. " +
            "See go/autofill-internal-parsing-patterns for more details. " +
            "Ideally, the legacy patterns should not be modified.")
    ]

  return []

def CheckModificationOfFormAutofillUtil(input_api, output_api):
  """Reminds to keep form_autofill_util.cc and the iOS counterpart in sync."""

  if (IsComponentsAutofillFileAffected(input_api, "fill.ts") !=
      IsComponentsAutofillFileAffected(input_api, "form_autofill_util.cc")):
    return [
        output_api.PresubmitNotifyResult(
            'Form extraction/label inference has a separate iOS ' +
            'implementation in components/autofill/ios/form_util/resources/' +
            'fill.ts. Try to keep it in sync with form_autofill_util.cc.')
    ]

  return []

# Checks that whenever the regex transpiler is modified, the golden test files
# are updated to match the new output. This serves as a testing mechanism for
# the transpiler.
def CheckRegexTranspilerGoldenFiles(input_api, output_api):
  if not IsComponentsAutofillFileAffected(input_api,
                                          "transpile_regex_patterns.py"):
    return []

  relative_test_dir = input_api.os_path.join(
    "components", "test", "data", "autofill", "regex-transpiler")
  test_dir = input_api.os_path.join(
    input_api.PresubmitLocalPath(), os.pardir, os.pardir, relative_test_dir)

  # Transpiles `test_dir/file_name` into `output_file`.
  def transpile(file_name, output_file):
    transpiler = input_api.os_path.join(input_api.PresubmitLocalPath(),
      "core", "browser", "form_parsing", "transpile_regex_patterns.py")
    input_file = input_api.os_path.join(test_dir, file_name)
    subprocess.run([input_api.python3_executable, transpiler,
      "--input", input_file, "--output", output_file])

  # Transpiles `test_name`.in and returns whether it matches `test_name`.out.
  def run_test(test_name):
    expected_output = input_api.os_path.join(test_dir, test_name + ".out")
    with input_api.CreateTemporaryFile() as transpiled_output:
      transpile(test_name + ".in", transpiled_output.name)
      return filecmp.cmp(transpiled_output.name, expected_output, shallow=False)

  tests = [name[:-3] for name in os.listdir(test_dir) if name.endswith(".in")]
  if not all(run_test(test) for test in tests):
    return [output_api.PresubmitError(
      "Regex transpiler golden files don't match. "
      "Regenerate the outputs at {}.".format(relative_test_dir))]
  return []

def CheckAutofillAiSchema(input_api, output_api):
  """Reminds to run update_autofill_enums.py when the schema changes."""
  if (IsComponentsAutofillFileAffected(input_api, "entity_schema.json")):
    return [
        output_api.PresubmitPromptWarning(
            'You modified the Autofill AI schema. If you added an entity,'
            ' re-run `tools/metrics/histograms/update_autofill_enums.py`.'
        )
    ]

  return []

def CheckFeatureFilesOrdering(input_api, output_api):
  """Checks that the base::Features are declared and defined in alphabetical
  order."""
  FEATURE_FILES = [
      "autofill_features.h",
      "autofill_features.cc",
      "autofill_debug_features.h",
      "autofill_debug_features.cc",
      "autofill_payments_features.h",
      "autofill_payments_features.cc",
  ]

  def validate_ordering(file):
    text = input_api.ReadFile(file)
    pattern = re.compile(
        r'(?:BASE_FEATURE|BASE_DECLARE_FEATURE)\s*\(\s*(\S+)\s*(?:,|\))',
        re.DOTALL,
    )
    features = pattern.findall(text)

    # Check for violations by comparing adjacent elements.
    return [x for x in zip(features[:-1], features[1:]) if x[0] > x[1]]

  errors = []
  for file in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if file.LocalPath().startswith('components/autofill/') and any(
        file.LocalPath().endswith(file_name) for file_name in FEATURE_FILES
    ):
      violations = validate_ordering(file)
      if violations:
        readable_violations = [
            f"\n`{rhs}` should come before `{lhs}`" for lhs, rhs in violations
        ]
        errors.append(
            output_api.PresubmitError(
                f'Keep the base::Features in {file} sorted.'
                f" Violations:{''.join(readable_violations)}"
            )
        )
  return errors
