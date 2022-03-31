// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/check_for_updates_task.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace updater {
namespace {

bool ShouldSkipCheck(scoped_refptr<Configurator> config,
                     scoped_refptr<updater::PersistedData> persisted_data) {
  // Skip if periodic updates are disabled altogether.
  const int check_delay_seconds = config->NextCheckDelay();
  if (check_delay_seconds == 0) {
    VLOG(0) << "Skipping checking for updates: NextCheckDelay is 0.";
    return true;
  }

  // Skip if the most recent check was too recent (and not in the future).
  const base::TimeDelta time_since_update =
      base::Time::NowFromSystemTime() - persisted_data->GetLastChecked();
  if (base::TimeDelta() < time_since_update &&
      time_since_update < base::Seconds(check_delay_seconds)) {
    VLOG(0) << "Skipping checking for updates: last update was "
            << time_since_update.InMinutes() << " minutes ago.";
    return true;
  }

  // Skip if the updater is in the update suppression period.
  UpdatesSuppressedTimes suppression;
  if (config->GetPolicyService()->GetUpdatesSuppressedTimes(nullptr,
                                                            &suppression) &&
      suppression.valid()) {
    base::Time::Exploded now;
    base::Time::Now().LocalExplode(&now);
    if (suppression.contains(now.hour, now.minute)) {
      VLOG(0) << "Skipping checking for updates: in update suppression period.";
      return true;
    }
  }

  return false;
}

}  // namespace

CheckForUpdatesTask::CheckForUpdatesTask(
    scoped_refptr<Configurator> config,
    base::OnceCallback<void(UpdateService::Callback)> update_checker)
    : config_(config),
      update_checker_(std::move(update_checker)),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      update_client_(update_client::UpdateClientFactory(config_)) {}

CheckForUpdatesTask::~CheckForUpdatesTask() = default;

void CheckForUpdatesTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldSkipCheck(config_, persisted_data_)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(update_checker_),
          base::BindOnce(
              [](base::OnceClosure closure, UpdateService::Result result) {
                VLOG(0) << "Check for update task complete: " << result;
                std::move(closure).Run();
              },
              std::move(callback))),
      base::Seconds(config_->InitialDelay()));
}

}  // namespace updater
