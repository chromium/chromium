// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/remove_uninstalled_apps_task.h"

#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"

namespace updater {

namespace {

bool PathOwnedByRoot(const base::FilePath& path) {
  base::stat_wrapper_t stat_info = {};
  if (base::File::Lstat(path, &stat_info) != 0) {
    VPLOG(1) << "Failed to get information on path " << path.value();
    return false;
  }
  return stat_info.st_uid == 0;
}

}  // namespace

std::optional<int> RemoveUninstalledAppsTask::GetUnregisterReason(
    const std::string& /*app_id*/,
    const base::FilePath& ecp) const {
  if (ecp.empty()) {
    return std::nullopt;
  }
  if (!base::PathExists(ecp)) {
    return std::make_optional(kUninstallPingReasonUninstalled);
  }
  if (scope_ == UpdaterScope::kUser && PathOwnedByRoot(ecp)) {
    return std::make_optional(kUninstallPingReasonUserNotAnOwner);
  }
  return std::nullopt;
}

}  // namespace updater
