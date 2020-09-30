// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/control_service_in_process.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_in_process.h"
#include "components/prefs/pref_service.h"

namespace updater {

ControlServiceInProcess::ControlServiceInProcess(
    scoped_refptr<updater::Configurator> config)
    : config_(config),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

void ControlServiceInProcess::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UnregisterMissingApps();
  MaybeCheckForUpdates(std::move(callback));
}

void ControlServiceInProcess::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  std::move(callback).Run();
}

void ControlServiceInProcess::MaybeCheckForUpdates(base::OnceClosure callback) {
  const base::Time lastUpdateTime =
      config_->GetPrefService()->GetTime(kPrefUpdateTime);

  const base::TimeDelta timeSinceUpdate =
      base::Time::NowFromSystemTime() - lastUpdateTime;
  if (base::TimeDelta() < timeSinceUpdate &&
      timeSinceUpdate <
          base::TimeDelta::FromSeconds(config_->NextCheckDelay())) {
    VLOG(0) << "Skipping checking for updates:  "
            << timeSinceUpdate.InMinutes();
    main_task_runner_->PostTask(FROM_HERE, std::move(callback));
    return;
  }

  scoped_refptr<UpdateServiceInProcess> update_service =
      base::MakeRefCounted<UpdateServiceInProcess>(config_);

  update_service->UpdateAll(
      base::BindRepeating([](UpdateService::UpdateState) {}),
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
          base::BindOnce(std::move(callback)), config_));
}

void ControlServiceInProcess::UnregisterMissingApps() {
  for (const auto& app_id : persisted_data_->GetAppIds()) {
    // Skip if app_id is equal to updater app id.
    if (app_id == kUpdaterAppId)
      continue;

    const base::FilePath ecp = persisted_data_->GetExistenceCheckerPath(app_id);
    if (!ecp.empty() && !base::PathExists(ecp)) {
      if (!persisted_data_->RemoveApp(app_id))
        VLOG(0) << "Could not remove registration of app " << app_id;
    }
  }
}

void ControlServiceInProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefsCommitPendingWrites(config_->GetPrefService());
}

ControlServiceInProcess::~ControlServiceInProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
