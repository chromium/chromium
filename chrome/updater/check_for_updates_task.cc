// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/check_for_updates_task.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace updater {
namespace {

bool ShouldSkipCheck(scoped_refptr<Configurator> config) {
  // Skip if periodic updates are disabled altogether.
  const int check_delay_seconds = config->NextCheckDelay();
  if (check_delay_seconds == 0) {
    VLOG(0) << "Skipping checking for updates: NextCheckDelay is 0.";
    return true;
  }

  // Skip if the most recent check was too recent (and not in the future).
  const base::TimeDelta time_since_update =
      base::Time::NowFromSystemTime() -
      config->GetPrefService()->GetTime(kPrefUpdateTime);
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
    base::OnceCallback<void(UpdateService::Callback)> update_checker,
    base::OnceClosure callback)
    : config_(config),
      update_checker_(std::move(update_checker)),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      update_client_(update_client::UpdateClientFactory(config_)),
      callback_(std::move(callback)),
      number_of_pings_remaining_(0) {}

CheckForUpdatesTask::~CheckForUpdatesTask() = default;

std::vector<CheckForUpdatesTask::AppInfo>
CheckForUpdatesTask::GetRegisteredApps() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<AppInfo> apps_to_unregister;
  for (const std::string& app_id : persisted_data_->GetAppIds()) {
    if (app_id == kUpdaterAppId)
      continue;

    const base::FilePath ecp = persisted_data_->GetExistenceCheckerPath(app_id);
    if (!ecp.empty()) {
      apps_to_unregister.push_back(
          AppInfo(app_id, persisted_data_->GetProductVersion(app_id), ecp));
    }
  }
  return apps_to_unregister;
}

bool CheckForUpdatesTask::WaitingOnUninstallPings() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return number_of_pings_remaining_ > 0;
}

void CheckForUpdatesTask::MaybeCheckForUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldSkipCheck(config_)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&CheckForUpdatesTask::MaybeCheckForUpdatesDone, this));
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          std::move(update_checker_),
          base::BindOnce(
              [](base::OnceClosure closure, scoped_refptr<Configurator> config,
                 UpdateService::Result result) {
                const int exit_code = static_cast<int>(result);
                VLOG(0) << "Check for update task complete: exit_code = "
                        << exit_code;
                if (result == UpdateService::Result::kSuccess) {
                  config->GetPrefService()->SetTime(
                      kPrefUpdateTime, base::Time::NowFromSystemTime());
                }
                std::move(closure).Run();
              },
              base::BindOnce(&CheckForUpdatesTask::MaybeCheckForUpdatesDone,
                             this),
              config_)),
      base::Seconds(config_->InitialDelay()));
}

void CheckForUpdatesTask::MaybeCheckForUpdatesDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run();
}

void CheckForUpdatesTask::UnregisterMissingApps() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckForUpdatesTask::GetAppIDsToRemove, this,
                     GetRegisteredApps()),
      base::BindOnce(&CheckForUpdatesTask::RemoveAppIDsAndSendUninstallPings,
                     this));
}

void CheckForUpdatesTask::UnregisterMissingAppsDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeCheckForUpdates();
}

std::vector<CheckForUpdatesTask::PingInfo>
CheckForUpdatesTask::GetAppIDsToRemove(const std::vector<AppInfo>& apps) {
  std::vector<PingInfo> app_ids_to_remove;
  for (const auto& app : apps) {
    // Skip if app_id is equal to updater app id.
    if (app.app_id_ == kUpdaterAppId)
      continue;

    if (!base::PathExists(app.ecp_)) {
      app_ids_to_remove.push_back(PingInfo(app.app_id_, app.app_version_,
                                           kUninstallPingReasonUninstalled));
    } else if (!PathOwnedByUser(app.ecp_)) {
      app_ids_to_remove.push_back(PingInfo(app.app_id_, app.app_version_,
                                           kUninstallPingReasonUserNotAnOwner));
    }
  }

  return app_ids_to_remove;
}

void CheckForUpdatesTask::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnregisterMissingApps();
}

void CheckForUpdatesTask::RemoveAppIDsAndSendUninstallPings(
    const std::vector<PingInfo>& app_ids_to_remove) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (app_ids_to_remove.empty()) {
    UnregisterMissingAppsDone();
    return;
  }

  for (const PingInfo& app_id_to_remove : app_ids_to_remove) {
    const std::string& app_id = app_id_to_remove.app_id_;
    const int ping_reason = app_id_to_remove.ping_reason_;
    const base::Version& app_version = app_id_to_remove.app_version_;
    const std::string& brand = persisted_data_->GetBrandCode(app_id);
    const std::string& ap = persisted_data_->GetAP(app_id);

    if (persisted_data_->RemoveApp(app_id)) {
      VLOG(1) << "Uninstall ping for app id: " << app_id
              << ". Ping reason: " << ping_reason;
      ++number_of_pings_remaining_;
      update_client::CrxComponent crx_component;
      crx_component.ap = ap;
      crx_component.app_id = app_id;
      crx_component.brand = brand;
      crx_component.version = app_version;
      crx_component.requires_network_encryption = false;
      update_client_->SendUninstallPing(
          crx_component, ping_reason,
          base::BindOnce(&CheckForUpdatesTask::UninstallPingSent, this));
    } else {
      VLOG(0) << "Could not remove registration of app " << app_id;
    }
  }
}

void CheckForUpdatesTask::UninstallPingSent(update_client::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --number_of_pings_remaining_;

  if (error != update_client::Error::NONE)
    VLOG(0) << __func__ << ": Error: " << static_cast<int>(error);

  if (!WaitingOnUninstallPings())
    std::move(
        base::BindOnce(&CheckForUpdatesTask::UnregisterMissingAppsDone, this))
        .Run();
}

}  // namespace updater
