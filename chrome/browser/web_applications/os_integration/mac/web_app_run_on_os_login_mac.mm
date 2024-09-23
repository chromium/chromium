// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include "base/functional/bind.h"
#import "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app::internals {

void RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info,
                          ResultCallback callback) {
  base::FilePath shortcut_data_dir = GetShortcutDataDir(shortcut_info);

  ShortcutLocations locations;
  locations.in_startup = true;

  CreatePlatformShortcuts(
      shortcut_data_dir, locations, SHORTCUT_CREATION_AUTOMATED, shortcut_info,
      base::BindOnce([](bool shortcut_created) {
        return shortcut_created ? Result::kOk : Result::kError;
      }).Then(std::move(callback)));
}

Result UnregisterRunOnOsLogin(const std::string& app_id,
                              const base::FilePath& profile_path,
                              const std::u16string& shortcut_title) {
  RemoveAppShimFromLoginItems(app_id);
  return Result::kOk;
}

}  // namespace web_app::internals
