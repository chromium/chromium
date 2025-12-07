# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = "2.0.0"


def CheckProtoVisitorChange(input_api, output_api):
    files = input_api.LocalPaths()
    if (any(f.endswith(".proto") for f in files)
            and all(not f.endswith("proto_visitors.h") for f in files)):
        return [
            output_api.PresubmitPromptWarning(
                "You changed proto files, but didn\'t change proto_visitors.h")
        ]

    return []
