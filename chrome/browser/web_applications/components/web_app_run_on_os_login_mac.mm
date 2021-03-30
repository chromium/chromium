// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"

#import "chrome/browser/web_applications/components/web_app_shortcut_mac.h"

namespace web_app {

namespace internals {

bool RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info) {
  base::FilePath shortcut_data_dir = GetShortcutDataDir(shortcut_info);

  ShortcutLocations locations;
  locations.in_startup = true;

  return CreatePlatformShortcuts(shortcut_data_dir, locations,
                                 SHORTCUT_CREATION_AUTOMATED, shortcut_info);
}

bool UnregisterRunOnOsLogin(const std::string& app_id,
                            const base::FilePath& profile_path,
                            const std::u16string& shortcut_title) {
  RemoveAppShimFromLoginItems(app_id);
  return true;
}

}  // namespace internals

}  // namespace web_app
