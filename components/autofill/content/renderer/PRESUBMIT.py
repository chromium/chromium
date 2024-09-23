# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/components/autofill/content/renderer.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

def CheckNoBannedFunctions(input_api, output_api):
    """Makes sure that banned functions are not used."""
    errors = []
    file_filter = lambda f: (
        f.LocalPath().startswith('components/autofill/content/renderer/')
        and f.LocalPath().endswith(('.h', '.cc'))
    )
    banned_functions = [
        (r'\bFormControlType\(\)',
         'Consider FormControlTypeForAutofill() instead.'),
        (r'\bForm\(\)',
         'Consider GetOwningForm() instead.'),
        (r'\bGetFormControlElements\(\)',
         'Consider GetOwnedFormControls() instead.'),
        (r'\bUnassociatedFormControls\(\)',
         'Consider GetOwnedFormControls() instead.'),
    ]
    for f in input_api.AffectedSourceFiles(file_filter):
        for line_num, line in f.ChangedContents():
            if line.endswith("// nocheck"):
                continue
            line = line.split('//')[0]
            for regex, explanation in banned_functions:
                match = input_api.re.search(regex, line)
                if match:
                    errors.append(
                        output_api.PresubmitError(
                            f'{f.LocalPath()}:{line_num}: {match.group(0)}: '
                            f'{explanation} '
                            f'Or append // nocheck if you have to.'
                        )
                    )
    return errors
