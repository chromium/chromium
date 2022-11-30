// Copyright 2020 The Chromium Authors
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

namespace full_restore {

// Saves the app launch parameters to the full restore file.
COMPONENT_EXPORT(APP_RESTORE)
void SaveAppLaunchInfo(
    const base::FilePath& profile_path,
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info);

// Saves the window information to the full restore file.
COMPONENT_EXPORT(APP_RESTORE)
void SaveWindowInfo(const app_restore::WindowInfo& window_info);

// Sets the current active profile path.
COMPONENT_EXPORT(APP_RESTORE)
void SetActiveProfilePath(const base::FilePath& profile_path);

// Sets the primary user profile path.
COMPONENT_EXPORT(APP_RESTORE)
void SetPrimaryProfilePath(const base::FilePath& profile_path);

// Returns true if there are app type browsers from the full restore file.
// Otherwise, returns false.
COMPONENT_EXPORT(APP_RESTORE)
bool HasAppTypeBrowser(const base::FilePath& profile_path);

// Returns true if there are normal browsers from the full restore file.
// Otherwise, returns false.
COMPONENT_EXPORT(APP_RESTORE)
bool HasBrowser(const base::FilePath& profile_path);

COMPONENT_EXPORT(APP_RESTORE)
void AddChromeBrowserLaunchInfoForTesting(const base::FilePath& profile_path);

// Returns the full restore app id that's associated with |window|. Note this
// might be different with the window's kAppIdKey property value. kAppIdKey
// is only non-empty for Chrome apps and ARC apps, but empty for other apps (for
// example, browsers, PWAs, SWAs). This function however guarantees to return a
// valid app id value for a window that can be restored by full restore.
COMPONENT_EXPORT(APP_RESTORE)
std::string GetAppId(aura::Window* window);

// Invoked when an Chrome app Lacros window is created. `app_id` is the
// AppService id, and `window_id` is the wayland app_id property for the window.
COMPONENT_EXPORT(APP_RESTORE)
void OnLacrosChromeAppWindowAdded(const std::string& app_id,
                                  const std::string& window_id);

// Invoked when an Chrome app Lacros window is removed. `app_id` is the
// AppService id, and `window_id` is the wayland app_id property for the window.
COMPONENT_EXPORT(APP_RESTORE)
void OnLacrosChromeAppWindowRemoved(const std::string& app_id,
                                    const std::string& window_id);

}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_FULL_RESTORE_UTILS_H_
