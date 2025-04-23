// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

class UsageStatsProviderImpl : public UsageStatsProvider {
 public:
  UsageStatsProviderImpl() = default;

  // TODO(crbug.com/40821596): Implement.
  bool AnyAppEnablesUsageStats() const override { return false; }
  bool RemoteEventLoggingAllowed() const override { return false; }
};

std::unique_ptr<UsageStatsProvider> UsageStatsProvider::Create(
    UpdaterScope scope) {
  return std::make_unique<UsageStatsProviderImpl>();
}

}  // namespace updater
