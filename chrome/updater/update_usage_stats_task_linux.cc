// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <string>
#include <vector>

namespace updater {

bool OtherAppUsageStatsAllowed(const std::vector<std::string>& app_ids,
                               UpdaterScope scope) {
  // TODO(crbug.com/40821596): Implement.
  return false;
}

bool AreRawUsageStatsEnabled(
    UpdaterScope scope,
    const std::vector<std::string>& include_only_these_app_ids) {
  return false;
}

}  // namespace updater
