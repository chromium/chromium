# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', '..', 'tools')]
    import web_dev_style.presubmit_support
    results += web_dev_style.presubmit_support.CheckStyleESLint(
        input_api, output_api)
  finally:
    sys.path = old_sys_path
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True,
                                                         check_python=False)
  return results

def CheckTestFilename(input_api, output_api):
  results = []

  def IsNameInvalid(affected_file):
    return affected_file.LocalPath().endswith('_tests.ts')

  invalid_test_files = input_api.AffectedFiles(include_deletes=False,
                                               file_filter=IsNameInvalid)
  for f in invalid_test_files:
    results += [
        output_api.PresubmitError(
            f'Disallowed \'_tests\' suffix found in \'{f}\'. WebUI test files '
            'must end with "_test" suffix instead.')
    ]

  return results

def CheckPreferDisablingTestCasesOverSuites(input_api, output_api):
  """Checks that test suites aren't marked as DISABLED_."""
  results = []

  # Allow bypassing with a tag in the CL description.
  if input_api.change.tags.get("SKIP_DISABLING_SUITES_CHECK"):
    return results

  def IsCppTestFile(affected_file):
    path = affected_file.LocalPath()
    return path.endswith("test.cc")

  disabled_test_re = input_api.re.compile(r"\bDISABLED_")

  for f in input_api.AffectedFiles(
      include_deletes=False, file_filter=IsCppTestFile):
    old_contents = f.OldContents()
    for line_num, line in enumerate(f.NewContents(), start=1):
      if disabled_test_re.search(line) and line not in old_contents:
        results.append(
            output_api.PresubmitPromptWarning(
                f'New "DISABLED_" test found in {f.LocalPath()}:{line_num}. '
                "Prefer disabling individual test cases in the Mocha test file "
                "using test.skip() or <if expr> instead of disabling the entire "
                "C++ test suite. See https://chromium.googlesource.com/chromium/src/+/main/docs/webui/testing_webui.md#disabling-tests "
                "for more information. This can be bypassed by adding "
                '"SKIP_DISABLING_SUITES_CHECK: <reason>" to the CL description.'
            )
        )

  return results
