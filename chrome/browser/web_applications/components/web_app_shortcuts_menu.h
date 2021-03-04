// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUTS_MENU_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUTS_MENU_H_

#include <vector>

#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"

namespace base {
class FilePath;
}

namespace web_app {

// Returns true if Shortcuts Menu is managed externally by the operating system,
// and Chrome supports Shortcuts Menu on this operating system.This does not
// take into account the state of kDesktopPWAsAppIconShortcutsMenu flag.
bool ShouldRegisterShortcutsMenuWithOs();

// Does an OS specific registration of a Shortcuts Menu for the web app's icon.
void RegisterShortcutsMenuWithOs(
    const AppId& app_id,
    const base::FilePath& profile_path,
    const base::FilePath& shortcut_data_dir,
    const std::vector<WebApplicationShortcutsMenuItemInfo>&
        shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps);

// Deletes the ShortcutsMenu from the OS. This should be called during the
// uninstallation process. Returns true on successful deletion.
bool UnregisterShortcutsMenuWithOs(const AppId& app_id,
                                   const base::FilePath& profile_path);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUTS_MENU_H_
