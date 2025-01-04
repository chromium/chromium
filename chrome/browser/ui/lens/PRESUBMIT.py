# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckUnintentionalLogging(input_api, output_api):
    files_with_logs = set([])

    # Allow skipping this check if needed.
    if input_api.change.tags.get('SKIP_LOG_CHECK'):
        return []

    for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
        for _, line in f.ChangedContents():
            if input_api.re.search(r"\bD?V?LOG\s*\(\s*\w*\s*\)", line):
                files_with_logs.add(f.LocalPath())
            elif input_api.re.search(r"\bD?V?LOG_IF\s*\(\s*\w*\s*,", line):
                files_with_logs.add(f.LocalPath())

    if files_with_logs:
        return [
            output_api.PresubmitError(
                'Your changelist contains logging statements. Please remove '
                'them. \nIf these logs are intentional, add '
                'SKIP_LOG_CHECK=<reason> to the CL description.\n Logs found '
                'in: ',
                items=files_with_logs)
        ]
    return []
