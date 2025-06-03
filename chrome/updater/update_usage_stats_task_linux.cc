// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

bool AnyAppEnablesUsageStats(UpdaterScope scope) {
  return false;
}

bool RemoteEventLoggingAllowed(UpdaterScope, std::optional<std::string>) {
  return false;
}

}  // namespace updater
