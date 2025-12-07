# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for Chrome Enterprise E2E tests.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'
_OTA_DOC_LINK = 'https://g3doc.corp.google.com/googleclient/chrome/enterprise/g3doc/celab/write_enterprise_test.md?cl=head#testing-cloud-user-policies'


def CheckPylint(input_api, output_api):
  disabled_warnings = [
      # TODO(crbug.com/413421824): Burn down this list over time.
      'cyclic-imports',
      'expression-not-assigned',
      'logging-fstring-interpolation',
      'logging-format-interpolation',
      'no-else-return',
      'redefined-builtin',
      'redefined-outer-name',
      'unused-argument',
      'unused-outer-name',
      # No plans to fix these. Wildcards are part of the test discovery
      # mechanism.
      'wildcard-import',
      'unused-wildcard-import',
  ]
  check = input_api.canned_checks.GetPylint(
      input_api, output_api, disabled_warnings=disabled_warnings)
  return input_api.RunTests(check)


def CheckAccountsBelongToPool(input_api, output_api):
  account_pattern = input_api.re.compile(r'[\w\-]+@chromepizzatest.com')
  user_pattern = input_api.re.compile(r'account\d+')
  bad_accounts, locations = [], []
  for affected_file in input_api.AffectedTestableFiles():
    for (line_num, text) in affected_file.ChangedContents():
      account_match = account_pattern.search(text)
      if not account_match:
        continue
      if not user_pattern.match(account_match.group()):
        bad_accounts.append(account_match.group())
        start_col, end_col = account_match.span()
        location = output_api.PresubmitResultLocation(
            file_path=affected_file.LocalPath(),
            start_line=line_num,
            end_line=line_num,
            start_col=start_col,
            end_col=end_col)
        locations.append(location)

  if not bad_accounts:
    return []
  return [
      output_api.PresubmitPromptWarning(
          message=(
              "This CL appears to use accounts that aren't OTAs from the shared "
              'pool. Please consider doing so to avoid blocked logins.'),
          items=bad_accounts,
          long_text=_OTA_DOC_LINK,
          locations=locations,
      )
  ]
