// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <string>
#include <vector>

namespace updater {

bool OtherAppUsageStatsAllowed(const std::vector<std::string>& app_ids,
                               UpdaterScope scope) {
  // TODO(crbug.com/1296311): Implement.
  return false;
}

}  // namespace updater
