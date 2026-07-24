# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //chrome/test.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

# BUILD.gn files that are edited by many concurrent CLs. The global
# CheckPatchFormatted() in the root PRESUBMIT.py only emits a warning, which is
# easy to miss; when unformatted edits to these large, high-traffic files land
# close together they cause `gn format` churn and needless merge conflicts.
# Enforce `git cl format` as an error on these specific files only, to keep the
# blast radius small.
_FORMAT_REQUIRED_FILES = ('chrome/test/BUILD.gn', )


def CheckHighTrafficBuildGnFormatted(input_api, output_api):
    return input_api.canned_checks.CheckPatchFormatted(
        input_api,
        output_api,
        result_factory=output_api.PresubmitError,
        file_filter=lambda f: f.LocalPath() in _FORMAT_REQUIRED_FILES)
