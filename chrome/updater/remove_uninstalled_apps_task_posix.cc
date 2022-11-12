// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/remove_uninstalled_apps_task.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

absl::optional<int> RemoveUninstalledAppsTask::GetUnregisterReason(
    const std::string& /*app_id*/,
    const base::FilePath& ecp) const {
  if (ecp.empty()) {
    return absl::nullopt;
  }
  if (!base::PathExists(ecp)) {
    return absl::make_optional(kUninstallPingReasonUninstalled);
  }
  if (!PathOwnedByUser(ecp)) {
    return absl::make_optional(kUninstallPingReasonUserNotAnOwner);
  }
  return absl::nullopt;
}

}  // namespace updater
