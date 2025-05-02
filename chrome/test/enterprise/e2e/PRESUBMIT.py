# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for Chrome Enterprise E2E tests.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


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
