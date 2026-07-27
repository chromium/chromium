# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for chrome/test/mini_installer.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
    disabled_warnings = [
        'anomalous-backslash-in-string',
        'consider-using-in',
        'consider-using-with',
        'deprecated-module',
        'eval-used',
        'line-too-long',
        'logging-not-lazy',
        'misplaced-bare-raise',
        'missing-module-docstring',
        'protected-access',
        'superfluous-parens',
        'undefined-variable',
        'unspecified-encoding',
        'use-dict-literal',
    ]
    return input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        disabled_warnings=disabled_warnings,
        version='3.2')


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api)
