// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/check_for_updates_task.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace updater {
namespace {

bool ShouldSkipCheck(scoped_refptr<Configurator> config,
                     const std::string& task_name) {
  // To spread out synchronized load, sometimes use a higher delay.
  const base::TimeDelta check_delay =
      config->NextCheckDelay() * (base::RandDouble() < 0.1 ? 1.2 : 1);

  // Skip if periodic updates are disabled altogether, for instance, by an admin
  // setting `AutoUpdateCheckPeriodMinutes` to zero.
  if (check_delay.is_zero()) {
    VLOG(0) << "Skipping " << task_name << ": NextCheckDelay is 0.";
    return true;
  }

  // Skip if the most recent check was too recent (and not in the future).
  const base::TimeDelta time_since_update =
      base::Time::NowFromSystemTime() -
      config->GetUpdaterPersistedData()->GetLastChecked();
  if (time_since_update.is_positive() && time_since_update < check_delay) {
    VLOG(0) << "Skipping " << task_name << ": last update was "
            << time_since_update.InMinutes()
            << " minutes ago. check_delay == " << check_delay.InMinutes();
    return true;
  }

  // Skip if the updater is in the update suppression period.
  return config->GetPolicyService()->AreUpdatesSuppressedNow();
}

}  // namespace

CheckForUpdatesTask::CheckForUpdatesTask(scoped_refptr<Configurator> config,
                                         UpdaterScope scope,
                                         const std::string& task_name,
                                         UpdateChecker update_checker)
    : config_(config),
      task_name_(task_name),
      update_checker_(std::move(update_checker)),
      update_client_(update_client::UpdateClientFactory(config_)) {}

CheckForUpdatesTask::~CheckForUpdatesTask() = default;

void CheckForUpdatesTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldSkipCheck(config_, task_name_)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(update_checker_),
          base::BindOnce(
              [](base::OnceClosure closure, const std::string& task_name,
                 UpdateService::Result result) {
                VLOG(0) << task_name << " task complete: " << result;
                std::move(closure).Run();
              },
              std::move(callback), task_name_)),
      config_->InitialDelay());
}

}  // namespace updater
