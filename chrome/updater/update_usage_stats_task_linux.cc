// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

bool AnyAppUsageStatsAllowed(UpdaterScope scope) {
  // TODO(crbug.com/40821596): Implement.
  return false;
}

void UpdateUsageStatsTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AnyAppUsageStatsAllowed, scope_),
      base::BindOnce(&UpdateUsageStatsTask::SetUsageStatsEnabled, this,
                     persisted_data_)
          .Then(std::move(callback)));
}

}  // namespace updater
