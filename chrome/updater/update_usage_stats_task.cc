// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace updater {
namespace {

void SetUsageStatsEnabled(scoped_refptr<PersistedData> persisted_data,
                          bool enabled) {
  persisted_data->SetUsageStatsEnabled(enabled);
  CrashClient::GetInstance()->SetUploadsEnabled(enabled);
}

}  // namespace

UpdateUsageStatsTask::UpdateUsageStatsTask(
    UpdaterScope scope,
    scoped_refptr<PersistedData> persisted_data)
    : scope_(scope), persisted_data_(persisted_data) {}

UpdateUsageStatsTask::~UpdateUsageStatsTask() = default;

void UpdateUsageStatsTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OtherAppUsageStatsAllowed, persisted_data_->GetAppIds(),
                     scope_),
      base::BindOnce(&SetUsageStatsEnabled, persisted_data_)
          .Then(std::move(callback)));
}

}  // namespace updater
