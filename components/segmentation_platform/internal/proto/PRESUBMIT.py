# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

def CheckChange(input_api, output_api):
    # Don't report the warning on --all and --files runs where nothing has
    # actually changed.
    if input_api.no_diffs:
        return []
    cwd = input_api.PresubmitLocalPath()
    for f in input_api.AffectedFiles():
        p = f.AbsoluteLocalPath()

        # Only do PRESUBMIT checks when |p| is under |cwd| and is a proto file.
        if input_api.os_path.commonprefix([p, cwd]) == cwd \
          and p.endswith('.proto'):
            return [
                output_api.PresubmitPromptWarning(
                    'Segmentation platform proto files were updated, consider '
                    + 'updating the VERSION field in components/' +
                    'segmentation_platform/internal/proto/model_metadata.proto.'
                )
            ]
    return []


def CheckChangeOnUpload(input_api, output_api):
    return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CheckChange(input_api, output_api)
