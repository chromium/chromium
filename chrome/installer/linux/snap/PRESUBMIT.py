# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for //chrome/installer/linux/snap."""

PRESUBMIT_VERSION = "2.0.0"


def CheckSnapBuildUnittests(input_api, output_api):
    # Snap packaging is Linux-only and relies on POSIX symlinks, chmod mode
    # bits (0755 vs 4755), and UTF-8 default text encoding in installer.py.
    if input_api.is_windows:
        return []
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        [r"^build_unittest\.py$"],
        run_on_python2=False,
        skip_shebang_check=True,
    )
