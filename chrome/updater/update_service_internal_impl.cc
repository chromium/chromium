// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/rand_util.h"
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
namespace {

class CheckForUpdatesTask : public UpdateServiceInternalImpl::Task {
 public:
  CheckForUpdatesTask(scoped_refptr<updater::Configurator> config,
                      base::OnceClosure callback);
  void Run() override;

  // Provides a way to remove apps from the persisted data if the app is no
  // longer installed on the machine.
  void UnregisterMissingApps();

 private:
  ~CheckForUpdatesTask() override = default;

  struct AppInfo {
    AppInfo(const std::string& app_id,
            const base::Version& app_version,
            const base::FilePath& ecp)
        : app_id_(app_id), app_version_(app_version), ecp_(ecp) {}
    std::string app_id_;
    base::Version app_version_;
    base::FilePath ecp_;
  };

  struct PingInfo {
    PingInfo(const std::string& app_id,
             const base::Version& app_version,
             int ping_reason)
        : app_id_(app_id),
          app_version_(app_version),
          ping_reason_(ping_reason) {}
    std::string app_id_;
    base::Version app_version_;
    int ping_reason_;
  };

  // Returns a list of apps registered with the updater.
  std::vector<AppInfo> GetRegisteredApps();

  // Returns a list of apps that need to be unregistered.
  std::vector<PingInfo> GetAppIDsToRemove(const std::vector<AppInfo>& apps);

  // Callback to run after a `MaybeCheckForUpdates` has finished.
  // Triggers the completion of the whole task.
  void MaybeCheckForUpdatesDone();

  // Unregisters the apps in `app_ids_to_remove` and starts an update check
  // if necessary.
  void RemoveAppIDsAndSendUninstallPings(
      const std::vector<PingInfo>& app_ids_to_remove);

  // After an uninstall ping has been processed, reduces the number of pings
  // that we need to wait on before checking for updates.
  void UninstallPingSent(update_client::Error error);

  // Returns true if there are uninstall ping tasks which haven't finished.
  // Returns false if `number_of_pings_remaining_` is 0.
  // `number_of_pings_remaining_` is only updated on the tasks's sequence.
  bool WaitingOnUninstallPings() const;

  // Checks for updates of all registered applications if it has been longer
  // than the last check time by NextCheckDelay() amount defined in the
  // config.
  void MaybeCheckForUpdates();

  // Callback to run after `UnregisterMissingApps` has finished.
  // Triggers `MaybeCheckForUpdates`.
  void UnregisterMissingAppsDone();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<updater::Configurator> config_;
  scoped_refptr<updater::PersistedData> persisted_data_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  base::OnceClosure callback_;
  int number_of_pings_remaining_;
};

CheckForUpdatesTask::CheckForUpdatesTask(
    scoped_refptr<updater::Configurator> config,
    base::OnceClosure callback)
    : config_(config),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      update_client_(update_client::UpdateClientFactory(config_)),
      callback_(std::move(callback)),
      number_of_pings_remaining_(0) {}

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
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&CheckForUpdatesTask::MaybeCheckForUpdatesDone, this));
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &updater::UpdateService::UpdateAll, update_service, base::DoNothing(),
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
              base::BindOnce(&CheckForUpdatesTask::MaybeCheckForUpdatesDone,
                             this),
              config_)),
      UpdateCheckJitter());
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

    if (persisted_data_->RemoveApp(app_id)) {
      VLOG(1) << "Uninstall ping for app id: " << app_id
              << ". Ping reason: " << ping_reason;
      ++number_of_pings_remaining_;
      update_client_->SendUninstallPing(
          app_id, app_version, ping_reason,
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

}  // namespace

base::TimeDelta UpdateCheckJitter() {
  return base::TimeDelta::FromSecondsD(base::RandDouble() *
                                       kUpdateCheckJitterMultiplier);
}

UpdateServiceInternalImpl::UpdateServiceInternalImpl(
    scoped_refptr<updater::Configurator> config)
    : config_(config) {}

void UpdateServiceInternalImpl::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto task = base::MakeRefCounted<CheckForUpdatesTask>(
      config_, base::BindOnce(&UpdateServiceInternalImpl::TaskDone, this,
                              std::move(callback)));
  // Queues the task to be run. If no other tasks are running, runs the task.
  tasks_.push(task);
  if (tasks_.size() == 1)
    RunNextTask();
}

void UpdateServiceInternalImpl::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void UpdateServiceInternalImpl::TaskDone(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
  tasks_.pop();
  RunNextTask();
}

void UpdateServiceInternalImpl::RunNextTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tasks_.empty()) {
    tasks_.front()->Run();
  }
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
