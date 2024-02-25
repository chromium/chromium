// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUTS_MENU_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUTS_MENU_H_

#include <vector>

#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"

namespace base {
class FilePath;
}

namespace web_app {

using RegisterShortcutsMenuCallback = base::OnceCallback<void(Result result)>;

// Returns true if Shortcuts Menu is managed externally by the operating system,
// and Chrome supports Shortcuts Menu on this operating system.
bool ShouldRegisterShortcutsMenuWithOs();

// Does an OS specific registration of a Shortcuts Menu for the web app's icon.
void RegisterShortcutsMenuWithOs(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path,
    const base::FilePath& shortcut_data_dir,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    RegisterShortcutsMenuCallback callback);

// Deletes the ShortcutsMenu from the OS. This should be called during the
// uninstallation process. Returns true if there were no errors.
bool UnregisterShortcutsMenuWithOs(const webapps::AppId& app_id,
                                   const base::FilePath& profile_path,
                                   RegisterShortcutsMenuCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUTS_MENU_H_
