// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_linux.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

namespace internals {

void RegisterRunOnOsLogin(const ShortcutInfo& shortcut_info,
                          ResultCallback callback) {
  base::FilePath shortcut_data_dir = GetShortcutDataDir(shortcut_info);

  ShortcutLocations locations;
  locations.in_startup = true;

  CreatePlatformShortcuts(
      shortcut_data_dir, locations, SHORTCUT_CREATION_BY_USER, shortcut_info,
      base::BindOnce([](bool shortcut_created) {
        return shortcut_created ? Result::kOk : Result::kError;
      }).Then(std::move(callback)));
}

Result UnregisterRunOnOsLogin(const std::string& app_id,
                              const base::FilePath& profile_path,
                              const std::u16string& shortcut_title) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ShortcutLocations locations;
  locations.in_startup = true;

  std::vector<base::FilePath> all_shortcut_files =
      GetShortcutLocations(locations, profile_path, app_id);
  Result result = Result::kOk;
  for (const auto& shortcut_file : all_shortcut_files) {
    if (!base::DeleteFile(shortcut_file))
      result = Result::kError;
  }
  return result;
}

}  // namespace internals

}  // namespace web_app
