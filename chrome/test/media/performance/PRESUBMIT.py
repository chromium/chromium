# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for chrome/test/media/performance.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckUnitTests(input_api, output_api):
    # The media perf hosts are Linux/Mac only.
    if input_api.is_windows:
        return []
    return input_api.RunTests(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            input_api.PresubmitLocalPath(),
            files_to_check=[r'.+_unittest\.py$'],
        )
    )
