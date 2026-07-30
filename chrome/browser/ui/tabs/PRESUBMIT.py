# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

_TAB_FEATURES_CC = 'tab_features.cc'

# SupportsUserData-based helpers that TabFeatures may instantiate directly.
#
# These are rare, deliberate exceptions: each listed helper must also be
# attachable to WebContents that are NOT tabs, so its storage cannot move
# into TabFeatures. Do NOT add to this list unless the helper genuinely has
# non-tab WebContents users; a feature that is only ever tab-scoped must be
# owned by TabFeatures as a std::unique_ptr member instead (see
# ui/base/unowned_user_data/README.md). Additions are expected to be
# justified in the CL description and reviewed by chrome/browser/ui/tabs
# OWNERS.
_ALLOWED_CREATE_FOR_CALLS = (
    # The task manager tag is looked up from WebContents user data by
    # WebContentsTaskProvider, is swapped in place by WebAppTabHelper, and is
    # also attached to non-tab WebContents (e.g. payment handler WebViews) and
    # to Android tabs, so the WebContents must own it.
    'task_manager::WebContentsTags::CreateForTabContents',
)


def CheckNoCreateForInTabFeatures(input_api, output_api):
    """Prevents SupportsUserData-style CreateFor* calls in tab_features.cc.

    Tab-scoped features must be owned directly by TabFeatures as a
    std::unique_ptr member instead of attaching themselves to the
    WebContents via SupportsUserData. The only exceptions are the
    explicitly allowlisted helpers above, which must support non-tab
    WebContents as well.
    """
    results = []
    for f in input_api.AffectedFiles():
        if input_api.os_path.basename(f.LocalPath()) != _TAB_FEATURES_CC:
            continue
        for line_num, line in enumerate(f.NewContents(), start=1):
            if 'CreateFor' not in line:
                continue
            if any(allowed in line for allowed in _ALLOWED_CREATE_FOR_CALLS):
                continue
            results.append(
                output_api.PresubmitError(
                    '%s:%d: "CreateFor" indicates the SupportsUserData '
                    'anti-pattern. Tab-scoped features must be owned by '
                    'TabFeatures as a std::unique_ptr member instead. If '
                    'the helper must also support non-tab WebContents, add '
                    'it to _ALLOWED_CREATE_FOR_CALLS in '
                    'chrome/browser/ui/tabs/PRESUBMIT.py with justification.'
                    % (f.LocalPath(), line_num)))
    return results
