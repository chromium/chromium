// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_APP_RESTORE_UTILS_H_
#define COMPONENTS_APP_RESTORE_APP_RESTORE_UTILS_H_

#include "base/component_export.h"
#include "ui/views/widget/widget.h"

namespace app_restore {
class RestoreData;
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

// Returns true if `window` is an ARC window. Otherwise, returns false.
bool IsArcWindow(aura::Window* window);

// Returns true if `window` is a Lacros window. Otherwise, returns false.
bool IsLacrosWindow(aura::Window* window);

// Returns true if there is a window info for `restore_window_id` from desk
// templates or full restore, depending on which one is thought to be launching
// apps currently. Otherwise, returns false. This interface can't be used for
// ARC app windows.
COMPONENT_EXPORT(APP_RESTORE)
bool HasWindowInfo(int32_t restore_window_id);

// Applies properties from `window_info` to the given `property_handler`.
// This is called from `GetWindowInfo()` when window is
// created, or from the ArcReadHandler when a task is ready for a full
// restore window that has already been created.
// TODO(sammiequon): Change the two arguments to references.
COMPONENT_EXPORT(APP_RESTORE)
void ApplyProperties(WindowInfo* window_info,
                     ui::PropertyHandler* property_handler);

// Modifies `out_params` based on the window info associated with
// `restore_window_id`.
COMPONENT_EXPORT(APP_RESTORE)
void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params);

// Fetches the restore window id from the restore data for the given `app_id`.
// `app_id` should be a Chrome app id.
COMPONENT_EXPORT(APP_RESTORE)
int32_t FetchRestoreWindowId(const std::string& app_id);

// Generates the ARC session id (1,000,000,001 - INT_MAX) for restored ARC
// apps.
COMPONENT_EXPORT(APP_RESTORE) int32_t CreateArcSessionId();

// Sets `arc_session_id` for `window_id`. `arc session id` is assigned when ARC
// apps are restored.
COMPONENT_EXPORT(APP_RESTORE)
void SetArcSessionIdForWindowId(int32_t arc_session_id, int32_t window_id);

// Associates `desk_template_launch_id` with `arc_session_id`.
COMPONENT_EXPORT(APP_RESTORE)
void SetDeskTemplateLaunchIdForArcSessionId(int32_t arc_session_id,
                                            int32_t desk_template_launch_id);

// Returns the restore window id for the ARC app's `task_id`.
COMPONENT_EXPORT(APP_RESTORE)
int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id);

// Returns the restore window id for the ARC app's `session_id`.
COMPONENT_EXPORT(APP_RESTORE)
int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id);

// Remove the "_crx_" prefix from a given `app_name` to get the app id.
COMPONENT_EXPORT(APP_RESTORE)
std::string GetAppIdFromAppName(const std::string& app_name);

// Returns the Lacros window id for `window`.
const std::string GetLacrosWindowId(aura::Window* window);

// Returns the restore window id for the Lacros window with `lacros_window_id`.
COMPONENT_EXPORT(APP_RESTORE)
int32_t GetLacrosRestoreWindowId(const std::string& lacros_window_id);

// Returns a tuple containing the window count, tab count, and total count, in
// that order. Note that tab count data is not saved for full restore, which
// relies on session restore to restore tabs.
COMPONENT_EXPORT(APP_RESTORE)
std::tuple<int, int, int> GetWindowAndTabCount(const RestoreData& restore_data);

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_APP_RESTORE_UTILS_H_
