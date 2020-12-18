// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_

#include <memory>

#include "base/component_export.h"

class AccountId;

namespace aura {
class Window;
}

namespace base {
class FilePath;
}

namespace full_restore {

struct AppLaunchInfo;
struct WindowInfo;

// Saves the app launch parameters to the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
void SaveAppLaunchInfo(const base::FilePath& profile_dir,
                       std::unique_ptr<AppLaunchInfo> app_launch_info);

// Saves the window information to the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
void SaveWindowInfo(std::unique_ptr<WindowInfo> window_info);

// Gets the window information from the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
std::unique_ptr<WindowInfo> GetWindowInfo(aura::Window* window);

// Returns true if we should restore apps and pages based on the restore setting
// and the user's choice from the notification. Otherwise, returns false.
COMPONENT_EXPORT(FULL_RESTORE) bool ShouldRestore(const AccountId& account_id);

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
