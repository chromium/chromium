// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_FULL_RESTORE_UTILS_H_
#define COMPONENTS_APP_RESTORE_FULL_RESTORE_UTILS_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/class_property.h"
#include "ui/views/widget/widget.h"

namespace app_restore {
struct AppLaunchInfo;
struct WindowInfo;
}  // namespace app_restore

namespace aura {
class Window;
}

namespace base {
class FilePath;
}

namespace app_restore {

// For ARC session id, 1 ~ 1000000000 is used as the window id for all new app
// launching. 1000000001 - INT_MAX is used as the session id for all restored
// app launching read from the full restore file.
//
// Assuming each day the new windows launched account is 1M, the above scope is
// enough for 3 years (1000 days). So there should be enough number to be
// assigned for ARC session ids.
constexpr int32_t kArcSessionIdOffsetForRestoredLaunching = 1000000000;

// If the ARC task is not created when the window is initialized, set the
// restore window id as -1, to add the ARC app window to the hidden container.
constexpr int32_t kParentToHiddenContainer = -1;

}  // namespace app_restore

namespace full_restore {

// Saves the app launch parameters to the full restore file.
COMPONENT_EXPORT(APP_RESTORE)
void SaveAppLaunchInfo(
    const base::FilePath& profile_path,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info);

// Saves the window information to the full restore file.
COMPONENT_EXPORT(APP_RESTORE)
void SaveWindowInfo(const app_restore::WindowInfo& window_info);

// Gets the window information from the full restore file for |window|.
COMPONENT_EXPORT(APP_RESTORE)
std::unique_ptr<app_restore::WindowInfo> GetWindowInfo(aura::Window* window);

// Fetches the restore window id from the restore data for the given |app_id|.
// |app_id| should be a Chrome app id.
COMPONENT_EXPORT(APP_RESTORE)
int32_t FetchRestoreWindowId(const std::string& app_id);

// Returns the restore window id for the ARC app's |task_id|.
COMPONENT_EXPORT(APP_RESTORE)
int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id);

// Returns the restore window id for the ARC app's |session_id|.
COMPONENT_EXPORT(APP_RESTORE)
int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id);

// Sets the current active profile path.
COMPONENT_EXPORT(APP_RESTORE)
void SetActiveProfilePath(const base::FilePath& profile_path);

// Returns true if there are app type browsers from the full restore file.
// Otherwise, returns false.
COMPONENT_EXPORT(APP_RESTORE)
bool HasAppTypeBrowser(const base::FilePath& profile_path);

// Returns true if there are normal browsers from the full restore file.
// Otherwise, returns false.
COMPONENT_EXPORT(APP_RESTORE)
bool HasBrowser(const base::FilePath& profile_path);

// Returns true if there is a window info for |restore_window_id| from the full
// restore file. Otherwise, returns false. This interface can't be used for Arc
// app windows.
COMPONENT_EXPORT(APP_RESTORE)
bool HasWindowInfo(int32_t restore_window_id);

// Modifies `out_params` based on the window info associated with
// `restore_window_id`.
COMPONENT_EXPORT(APP_RESTORE)
void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params);

COMPONENT_EXPORT(APP_RESTORE)
void AddChromeBrowserLaunchInfoForTesting(const base::FilePath& profile_path);

// Returns the full restore app id that's associated with |window|. Note this
// might be different with the window's kAppIdKey property value. kAppIdKey
// is only non-empty for Chrome apps and ARC apps, but empty for other apps (for
// example, browsers, PWAs, SWAs). This function however guarantees to return a
// valid app id value for a window that can be restored by full restore.
COMPONENT_EXPORT(APP_RESTORE)
std::string GetAppId(aura::Window* window);

}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_FULL_RESTORE_UTILS_H_
