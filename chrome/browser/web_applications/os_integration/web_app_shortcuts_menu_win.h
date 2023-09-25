// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUTS_MENU_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUTS_MENU_WIN_H_

#include <string>

#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"

class ShellLinkItem;

namespace web_app {

using UpdateJumpListForTesting = base::RepeatingCallback<
    bool(std::wstring, const std::vector<scoped_refptr<ShellLinkItem>>&)>;

// Callback when jump list has been registered with Windows.
void SetUpdateJumpListForTesting(
    UpdateJumpListForTesting updateJumpListForTesting);

std::wstring GenerateAppUserModelId(const base::FilePath& profile_path,
                                    const webapps::AppId& app_id);

namespace internals {

// Deletes all .ico shortcuts menu icons that were written to disk at PWA
// install time. Call this when PWA is uninstalled on Windows.
bool DeleteShortcutsMenuIcons(const base::FilePath& web_app_path);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUTS_MENU_WIN_H_
