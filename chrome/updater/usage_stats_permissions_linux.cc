// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/usage_stats_permissions.h"

#include <optional>
#include <string>
#include <vector>

#include "chrome/updater/external_constants.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

bool AnyAppEnablesUsageStats(UpdaterScope scope) {
  return false;
}

bool RemoteEventLoggingAllowed(UpdaterScope,
                               const std::vector<std::string>&,
                               std::optional<EventLoggingPermissionProvider>) {
  return false;
}

}  // namespace updater
