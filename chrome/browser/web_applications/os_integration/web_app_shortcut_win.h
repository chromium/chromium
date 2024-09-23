// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_WIN_H_

#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"

#include "base/win/windows_types.h"

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

// Finds shortcuts in |shortcut_path| that match profile for |profile_path| and
// extension with title |shortcut_name|.
// If |shortcut_name| is empty, finds all shortcuts matching |profile_path|.
std::vector<base::FilePath> FindAppShortcutsByProfileAndTitle(
    const base::FilePath& shortcut_path,
    const base::FilePath& profile_path,
    const std::u16string& shortcut_name);

base::FilePath GetIconFilePath(const base::FilePath& web_app_path,
                               const std::u16string& title);

void OnShortcutInfoLoadedForSetRelaunchDetails(
    HWND hwnd,
    std::unique_ptr<ShortcutInfo> shortcut_info);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_WIN_H_
