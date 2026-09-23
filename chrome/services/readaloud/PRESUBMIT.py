# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for chrome/services/readaloud.

Reuses the shared ReadAloud assertion order checks defined in
chrome/browser/readaloud/common_checks.py.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckTestAssertionOrder(input_api, output_api):
    old_path = input_api.sys.path[:]
    try:
        input_api.sys.path.insert(0, input_api.change.RepositoryRoot())
        from chrome.browser.readaloud.common_checks import CheckTestAssertionOrder as check_order
        return check_order(input_api, output_api)
    finally:
        input_api.sys.path = old_path
