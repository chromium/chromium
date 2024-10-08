# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for android buildbot.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

# See third_party/depot_tools/presubmit_support.py
PRESUBMIT_VERSION = '2.0.0'

# TODO(b/349074194): Set to True once rust has been enabled for Cronet.
_ENABLE_PRESUBMIT = False


def CheckGenerateBuildScriptsOutput(input_api, output_api):
    presubmit_script_path = input_api.PresubmitLocalPath()
    if _ENABLE_PRESUBMIT:
        return input_api.RunTests(
            input_api.canned_checks.GetUnitTests(
                input_api,
                output_api,
                unit_tests=[
                    input_api.os_path.join(
                        presubmit_script_path, 'tests',
                        'generate_build_scripts_output_test.py'),
                    input_api.os_path.join(presubmit_script_path, 'tests',
                                           'gen_android_bp_test.py')
                ],
            ))
    else:
        return ()
