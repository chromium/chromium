# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckAccountCapabilityRolloutSafety(input_api, output_api):
    """
    Checks for modifications to account_capabilities_list.h and warns the user
    to ensure that the capability has been rolled out server-side.
    """
    for f in input_api.AffectedFiles():
        if f.LocalPath().endswith("account_capabilities_list.h"):
            return [
                output_api.PresubmitPromptWarning(
                    'This CL modifies account_capabilities_list.h. If you are'
                    ' adding a new capability or exposing an existing'
                    ' capability on new platforms, please ensure that the'
                    ' capability is fully rolled out server-side before'
                    ' submitting this CL (Googlers, see'
                    ' go/chrome-account-capability-rollout), or use the'
                    ' flag-guarded ACCOUNT_CAPABILITY_F definition.')
            ]

    return []
