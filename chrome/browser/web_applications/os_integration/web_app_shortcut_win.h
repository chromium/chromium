// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_WIN_H_

#include <optional>

#include "base/win/windows_types.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "components/webapps/common/web_app_id.h"

namespace base {
class FilePath;
}  // namespace base

namespace web_app {

namespace internals {

// Sanitizes |name| and returns a version of it that is safe to use as an
// on-disk file name.
base::FilePath GetSanitizedFileName(const std::u16string& name);

// Returns the Windows user-level shortcut paths that are specified in
// |creation_locations|.
std::vector<base::FilePath> GetShortcutPaths(
    const ShortcutLocations& creation_locations);

// Saves |image| to |icon_file| if the file is outdated. Returns true if
// icon_file is up to date or successfully updated.
// If |refresh_shell_icon_cache| is true, the shell's icon cache will be
// refreshed, ensuring the correct icon is displayed, but causing a flicker.
// Refreshing the icon cache is not necessary on shortcut creation as the shell
// will be notified when the shortcut is created.
// Creates the parent dir of icon_file, if it doesn't exist.
bool CheckAndSaveIcon(const base::FilePath& icon_file,
                      const gfx::ImageFamily& image,
                      bool refresh_shell_icon_cache);

// Finds shortcuts in `shortcut_path` that match profile for `profile_path` and
// extension with title `shortcut_name`, corresponding to app with `app_id`.
// The `app_id` check will only be done if it is specified, otherwise it will
// query on the basis of `shortcut_name` only, falling back to only
// matching `profile_path` if neither is available.
std::vector<base::FilePath> FindAppShortcutsByProfileAppIdAndTitle(
    const base::FilePath& shortcut_path,
    const base::FilePath& profile_path,
    std::optional<std::u16string> shortcut_name,
    std::optional<webapps::AppId> app_id);

base::FilePath GetIconFilePath(const base::FilePath& web_app_path,
                               const std::u16string& title);

void OnShortcutInfoLoadedForSetRelaunchDetails(
    HWND hwnd,
    std::unique_ptr<ShortcutInfo> shortcut_info);

// Returns whether the shortcut file is created for the current `app_id` and
// profile by matching `profile_path`. If `app_id` is not specified, only
// verifies that this is an "app" shortcut by checking if the shortcut is in
// the profile path.
bool IsAppShortcutForProfile(const base::FilePath& shortcut_file_name,
                             const base::FilePath& profile_path,
                             std::optional<webapps::AppId> app_id);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_WIN_H_
