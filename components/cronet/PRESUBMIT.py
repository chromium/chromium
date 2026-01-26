# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for src/components/cronet.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import sys
PRESUBMIT_VERSION = '2.0.0'

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname('__file__'), os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.breakages_constants as breakages_constants  # pylint: disable=wrong-import-position

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
