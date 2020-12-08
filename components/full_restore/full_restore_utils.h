// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_

#include <memory>

#include "base/component_export.h"

namespace base {
class FilePath;
}

namespace full_restore {

class AppLaunchInfo;

// Saves the app launch parameters to the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
void SaveAppLaunchInfo(const base::FilePath& profile_dir,
                       std::unique_ptr<AppLaunchInfo> app_launch_info);

// Returns true if we should restore apps and pages based on the restore setting
// and the user's choice from the notification. Otherwise, returns false.
COMPONENT_EXPORT(FULL_RESTORE) bool ShouldRestore();

// Sets whether we should restore apps and pages, based on the restore setting
// and the user's choice from the notification.
COMPONENT_EXPORT(FULL_RESTORE) void SetRestoreFlag(bool should_restore);

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
