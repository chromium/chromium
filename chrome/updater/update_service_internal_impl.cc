// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"

namespace updater {

UpdateServiceInternalImpl::UpdateServiceInternalImpl(
    scoped_refptr<updater::Configurator> config)
    : config_(config),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      update_client_(update_client::UpdateClientFactory(config_)),
      number_of_pings_remaining_(0) {}

void UpdateServiceInternalImpl::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = std::move(callback);
  UnregisterMissingApps(GetRegisteredApps());
}

void UpdateServiceInternalImpl::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

std::vector<UpdateServiceInternalImpl::AppInfo>
UpdateServiceInternalImpl::GetRegisteredApps() {
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

bool UpdateServiceInternalImpl::WaitingOnUninstallPings() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return number_of_pings_remaining_ > 0;
}

void UpdateServiceInternalImpl::MaybeCheckForUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<UpdateServiceImpl> update_service =
      base::MakeRefCounted<UpdateServiceImpl>(config_);

  const base::Time lastUpdateTime =
      config_->GetPrefService()->GetTime(kPrefUpdateTime);

  const base::TimeDelta timeSinceUpdate =
      base::Time::NowFromSystemTime() - lastUpdateTime;
  if (base::TimeDelta() < timeSinceUpdate &&
      timeSinceUpdate <
          base::TimeDelta::FromSeconds(config_->NextCheckDelay())) {
    VLOG(0) << "Skipping checking for updates:  "
            << timeSinceUpdate.InMinutes();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback_));
    return;
  }

  update_service->UpdateAll(
      base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure closure,
             scoped_refptr<updater::Configurator> config,
             UpdateService::Result result) {
            const int exit_code = static_cast<int>(result);
            VLOG(0) << "UpdateAll complete: exit_code = " << exit_code;
            if (result == UpdateService::Result::kSuccess) {
              config->GetPrefService()->SetTime(
                  kPrefUpdateTime, base::Time::NowFromSystemTime());
            }
            std::move(closure).Run();
          },
          base::BindOnce(std::move(callback_)), config_));
}

void UpdateServiceInternalImpl::UnregisterMissingApps(
    const std::vector<AppInfo>& apps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&UpdateServiceInternalImpl::GetAppIDsToRemove, this, apps),
      base::BindOnce(
          &UpdateServiceInternalImpl::RemoveAppIDsAndSendUninstallPings, this));
}

void UpdateServiceInternalImpl::UnregisterMissingAppsDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeCheckForUpdates();
}

std::vector<UpdateServiceInternalImpl::PingInfo>
UpdateServiceInternalImpl::GetAppIDsToRemove(const std::vector<AppInfo>& apps) {
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

void UpdateServiceInternalImpl::RemoveAppIDsAndSendUninstallPings(
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

    if (persisted_data_->RemoveApp(app_id)) {
      VLOG(1) << "Uninstall ping for app id: " << app_id
              << ". Ping reason: " << ping_reason;
      number_of_pings_remaining_++;
      update_client_->SendUninstallPing(
          app_id, app_version, ping_reason,
          base::BindOnce(&UpdateServiceInternalImpl::UninstallPingSent, this));
    } else {
      VLOG(0) << "Could not remove registration of app " << app_id;
    }
  }
}

void UpdateServiceInternalImpl::UninstallPingSent(update_client::Error error) {
  number_of_pings_remaining_--;

  if (error != update_client::Error::NONE)
    VLOG(0) << __func__ << ": Error: " << static_cast<int>(error);

  if (!WaitingOnUninstallPings())
    std::move(base::BindOnce(
                  &UpdateServiceInternalImpl::UnregisterMissingAppsDone, this))
        .Run();
}

void UpdateServiceInternalImpl::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefsCommitPendingWrites(config_->GetPrefService());
}

UpdateServiceInternalImpl::~UpdateServiceInternalImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
