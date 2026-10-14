// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_REMOTE_TABS_MENU_UTIL_H_
#define CHROME_BROWSER_UI_TABS_REMOTE_TABS_MENU_UTIL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics_action.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/window_open_disposition.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace sync_sessions {
class OpenTabsUIDelegate;
}  // namespace sync_sessions

namespace ui {
class ImageModel;
class SimpleMenuModel;
}  // namespace ui

// Helpers shared by the menus that list open tabs from the user's other
// devices: the History submenu (both app menu implementations) and the "Other
// device tabs" app menu entry.
namespace remote_tabs_menu_util {

// Returns false if tabs from other devices must not be offered for `profile`:
// tab sync is disabled by policy, or the signed-in user has explicitly turned
// history sync off.
bool ShouldShowTabsFromOtherDevices(Profile* profile);

// Returns the menu icon representing a device of `form_factor`.
ui::ImageModel GetDeviceFormFactorIcon(
    syncer::DeviceInfo::FormFactor form_factor);

// Sets the default favicon on the item with `command_id` in `menu_model` and
// asynchronously replaces it with the favicon of `url`, a tab from another
// device. `owner` is the model whose `ui::MenuModelDelegate` is told about the
// icon change; the pending request is dropped once `owner` is invalidated.
void AddRemoteTabFavicon(Profile* profile,
                         base::WeakPtr<ui::SimpleMenuModel> owner,
                         ui::SimpleMenuModel* menu_model,
                         int command_id,
                         const GURL& url);

// Opens the tab identified by `session_tag` and `tab_id` from another device
// in `browser`. Records `user_action` immediately before restoring the tab.
// Returns false if the tab is no longer known to `open_tabs`.
bool OpenForeignSessionTab(BrowserWindowInterface* browser,
                           sync_sessions::OpenTabsUIDelegate* open_tabs,
                           const std::string& session_tag,
                           SessionID tab_id,
                           WindowOpenDisposition disposition,
                           base::UserMetricsAction user_action);

}  // namespace remote_tabs_menu_util

#endif  // CHROME_BROWSER_UI_TABS_REMOTE_TABS_MENU_UTIL_H_
