// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/class_property.h"
#include "ui/views/widget/widget.h"

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

// A property key to indicate the id for the window to be saved in RestoreData.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<int32_t>* const kWindowIdKey;

// A property key to indicate the restore id for the window from RestoreData.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<int32_t>* const kRestoreWindowIdKey;

// A property key to store the app id.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<std::string*>* const kAppIdKey;

// Saves the app launch parameters to the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
void SaveAppLaunchInfo(const base::FilePath& profile_path,
                       std::unique_ptr<AppLaunchInfo> app_launch_info);

// Saves the window information to the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
void SaveWindowInfo(const WindowInfo& window_info);

// Gets the window information from the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
std::unique_ptr<WindowInfo> GetWindowInfo(int32_t restore_window_id);
COMPONENT_EXPORT(FULL_RESTORE)
std::unique_ptr<WindowInfo> GetWindowInfo(aura::Window* window);

// Fetches the restore window id from the restore data for the given |app_id|.
// |app_id| should be a Chrome app id.
COMPONENT_EXPORT(FULL_RESTORE)
int32_t FetchRestoreWindowId(const std::string& app_id);

// Returns true if we should restore apps and pages based on the restore setting
// and the user's choice from the notification. Otherwise, returns false.
COMPONENT_EXPORT(FULL_RESTORE) bool ShouldRestore(const AccountId& account_id);

// Sets the current active profile path.
COMPONENT_EXPORT(FULL_RESTORE)
void SetActiveProfilePath(const base::FilePath& profile_path);

// Returns true if there is a window info for |restore_window_id| from the full
// restore file. Otherwise, returns false.
COMPONENT_EXPORT(FULL_RESTORE)
bool HasWindowInfo(int32_t restore_window_id);

// Modifies |out_params| based on the window info associated with
// |restore_window_id|.
COMPONENT_EXPORT(FULL_RESTORE)
void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params);

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
