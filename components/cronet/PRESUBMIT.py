# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for src/components/cronet.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
PRESUBMIT_VERSION = '2.0.0'

# Avoid importing modules by modifying sys.path. This causes side effects that
# prevent other PRESUBMIT.py scripts from correctly resolving their dependencies.
# Reference: crbug.com/478930205 - https://ci.chromium.org/ui/p/chromium/builders/ci/linux-presubmit/36616/overview

def CheckPyLint(input_api, output_api):
    pylint_checks = input_api.canned_checks.GetPylint(input_api, output_api)
    return input_api.RunTests(pylint_checks)


def CheckUnittestsOnCommit(input_api, output_api):
    return input_api.RunTests(
        input_api.canned_checks.GetUnitTestsRecursively(
            input_api,
            output_api,
            os.path.join(input_api.change.RepositoryRoot(), 'components',
                         'cronet'),
            files_to_check=['.*test\\.py$'],
            files_to_skip=[]))
