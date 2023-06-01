# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CheckChangeOnUpload(input_api, output_api):
    results = []

    proto_path = 'components/safe_browsing/content/common/proto/download_file_types.proto'

    if proto_path in input_api.change.LocalPaths():
        results.append(
            output_api.PresubmitPromptWarning(
                'You modified one or more of the download file type protos '
                'in: \n  ' + proto_path + '\n'
                'Please ensure this change is backwards compatible.'))
    return results
