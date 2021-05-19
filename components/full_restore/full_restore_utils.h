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

// A property key to indicate the id for the window to be saved in RestoreData.
// For web apps, browser windows or Chrome app windows, this is the session id.
// For ARC apps, this is the task id.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<int32_t>* const kWindowIdKey;

// A property key to indicate the restore id for the window from RestoreData.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<int32_t>* const kRestoreWindowIdKey;

// A property key to store the app id.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<std::string*>* const kAppIdKey;

// A property key to store the activation index of an app. Used by ash to
// determine where to stack a window among its siblings. Also used to determine
// if a window is restored by the full restore process. Only a window, restored
// from the full restore file and read by FullRestoreReadHandler during the
// system startup phase, could have a kActivationIndexKey. This is cleared after
// the window been activated. A smaller index indicates a more recently used
// window. If this key is null, then the window was not launched from full
// restore, or it is longer treated like a full restore launched window (i.e.
// user clicked on it).
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<int32_t*>* const kActivationIndexKey;

// A property key to add the window to a hidden container, if the ARC task is
// not created when the window is initialized.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<bool>* const kParentToHiddenContainerKey;

// A property key indicating whether a window was launched from full restore.
// These windows will not be activatable until they are shown.
COMPONENT_EXPORT(FULL_RESTORE)
extern const ui::ClassProperty<bool>* const kLaunchedFromFullRestoreKey;

// Saves the app launch parameters to the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
void SaveAppLaunchInfo(const base::FilePath& profile_path,
                       std::unique_ptr<AppLaunchInfo> app_launch_info);

// Saves the window information to the full restore file.
COMPONENT_EXPORT(FULL_RESTORE)
void SaveWindowInfo(const WindowInfo& window_info);

// Gets the window information from the full restore file for |window|.
COMPONENT_EXPORT(FULL_RESTORE)
std::unique_ptr<WindowInfo> GetWindowInfo(aura::Window* window);

// Fetches the restore window id from the restore data for the given |app_id|.
// |app_id| should be a Chrome app id.
COMPONENT_EXPORT(FULL_RESTORE)
int32_t FetchRestoreWindowId(const std::string& app_id);

// Returns the restore window id for the ARC app's |task_id|.
COMPONENT_EXPORT(FULL_RESTORE)
int32_t GetArcRestoreWindowId(int32_t task_id);

// Returns true if we should restore apps and pages based on the restore setting
// and the user's choice from the notification. Otherwise, returns false.
COMPONENT_EXPORT(FULL_RESTORE) bool ShouldRestore(const AccountId& account_id);

// Returns true if the restore pref is 'Always' or 'Ask every time', as we
// could restore apps and pages based on the user's choice from the
// notification for |account_id|. Otherwise, returns false, when the restore
// pref is 'Do not restore'.
COMPONENT_EXPORT(FULL_RESTORE)
bool CanPerformRestore(const AccountId& account_id);

// Sets the current active profile path.
COMPONENT_EXPORT(FULL_RESTORE)
void SetActiveProfilePath(const base::FilePath& profile_path);

// Returns true if there is a window info for |restore_window_id| from the full
// restore file. Otherwise, returns false. This interface can't be used for Arc
// app windows.
COMPONENT_EXPORT(FULL_RESTORE)
bool HasWindowInfo(int32_t restore_window_id);

// Modifies `out_params` based on the window info associated with
// `restore_window_id`.
COMPONENT_EXPORT(FULL_RESTORE)
void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params);

// Invoked when the task is created for an ARC app.
COMPONENT_EXPORT(FULL_RESTORE)
void OnTaskCreated(const std::string& app_id,
                   int32_t task_id,
                   int32_t session_id);

// Invoked when the task is destroyed for an ARC app.
COMPONENT_EXPORT(FULL_RESTORE)
void OnTaskDestroyed(int32_t task_id);

// Invoked when the task theme colors are updated for an ARC app.
COMPONENT_EXPORT(FULL_RESTORE)
void OnTaskThemeColorUpdated(int32_t task_id,
                             uint32_t primary_color,
                             uint32_t status_bar_color);

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_UTILS_H_
