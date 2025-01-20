// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_UTIL_WIN_H_
#define CHROME_UPDATER_APP_APP_INSTALL_UTIL_WIN_H_

#include <windows.h>

#include "chrome/updater/app/app_install_progress.h"

namespace updater {

// The current UI shows to the user only one completion type, even though
// there could be multiple applications in a bundle, where each application
// could have a different completion type. The following array lists the
// completion codes from low priority to high priority. The completion type
// with highest priority will be shown to the user.
inline constexpr CompletionCodes kCompletionCodesActionPriority[] = {
    CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY,
    CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND,
    CompletionCodes::COMPLETION_CODE_SUCCESS,
    CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND,
    CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY,
    CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY,
    CompletionCodes::COMPLETION_CODE_RESTART_BROWSER,
    CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS,
    CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY,
    CompletionCodes::COMPLETION_CODE_REBOOT,
    CompletionCodes::COMPLETION_CODE_ERROR,
    CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL,
};

// |kCompletionCodesActionPriority| must have all the values in enumeration
// CompletionCodes. The enumeration value starts from 1 so the array size
// should match the last value in the enumeration.
static_assert(
    std::size(kCompletionCodesActionPriority) ==
        static_cast<size_t>(
            CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL),
    "completion code is missing");

// Returns the index of `code` within `kCompletionCodesActionPriority`.
int GetPriority(CompletionCodes code);

// Launches the post-install launch command lines for each app in `info`.
bool LaunchCmdLines(const ObserverCompletionInfo& info);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_UTIL_WIN_H_
