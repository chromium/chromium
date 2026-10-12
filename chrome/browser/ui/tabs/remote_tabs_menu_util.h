// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_REMOTE_TABS_MENU_UTIL_H_
#define CHROME_BROWSER_UI_TABS_REMOTE_TABS_MENU_UTIL_H_

#include "components/sync_device_info/device_info.h"

class Profile;

namespace ui {
class ImageModel;
}  // namespace ui

// Helpers shared by the menus that list open tabs from the user's other
// devices: the History submenu (both app menu implementations) and the "Device
// tabs" app menu entry.
namespace remote_tabs_menu_util {

// Returns false if tabs from other devices must not be offered for `profile`:
// tab sync is disabled by policy, or the signed-in user has explicitly turned
// history sync off.
bool ShouldShowTabsFromOtherDevices(Profile* profile);

// Returns the menu icon representing a device of `form_factor`.
ui::ImageModel GetDeviceFormFactorIcon(
    syncer::DeviceInfo::FormFactor form_factor);

}  // namespace remote_tabs_menu_util

#endif  // CHROME_BROWSER_UI_TABS_REMOTE_TABS_MENU_UTIL_H_
