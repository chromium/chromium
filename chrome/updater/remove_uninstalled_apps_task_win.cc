// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/remove_uninstalled_apps_task.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

std::optional<int> RemoveUninstalledAppsTask::GetUnregisterReason(
    const std::string& app_id,
    const base::FilePath& /*ecp*/) const {
  base::win::RegKey key;
  if (key.Open(IsSystemInstall(scope_) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
               GetAppClientsKey(app_id).c_str(),
               Wow6432(KEY_READ)) == ERROR_FILE_NOT_FOUND) {
    return std::make_optional(kUninstallPingReasonUninstalled);
  }
  return std::nullopt;
}

}  // namespace updater
