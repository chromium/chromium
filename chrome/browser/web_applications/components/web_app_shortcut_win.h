// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_WIN_H_

#include "chrome/browser/web_applications/components/web_app_shortcut.h"

#include "base/win/windows_types.h"

namespace web_app {

base::FilePath GetChromeProxyPath();

namespace internals {

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
bool CheckAndSaveIcon(const base::FilePath& icon_file,
                      const gfx::ImageFamily& image,
                      bool refresh_shell_icon_cache);

base::FilePath GetIconFilePath(const base::FilePath& web_app_path,
                               const base::string16& title);

void OnShortcutInfoLoadedForSetRelaunchDetails(
    HWND hwnd,
    std::unique_ptr<ShortcutInfo> shortcut_info);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_WIN_H_
