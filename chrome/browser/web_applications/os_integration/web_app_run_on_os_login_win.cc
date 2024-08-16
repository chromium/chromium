// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include <memory>

#include "base/files/file_util.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

namespace internals {

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
  ShortcutLocations all_shortcut_locations;
  all_shortcut_locations.in_startup = true;
  std::vector<base::FilePath> all_paths =
      GetShortcutPaths(all_shortcut_locations);
  Result result = Result::kOk;
  // Only Startup folder is the expected path to be returned in all_paths.
  for (const auto& path : all_paths) {
    // Find all app's shortcuts in Startup folder to delete.
    std::vector<base::FilePath> shortcut_files =
        FindAppShortcutsByProfileAndTitle(path, profile_path, shortcut_title);
    for (const auto& shortcut_file : shortcut_files) {
      if (!base::DeleteFile(shortcut_file))
        result = Result::kError;
    }
  }
  return result;
}

}  // namespace internals

}  // namespace web_app
