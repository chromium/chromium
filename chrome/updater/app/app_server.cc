// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/update_service_impl_inactive.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/update_service_internal_impl.h"
#include "chrome/updater/update_service_internal_impl_inactive.h"
#include "chrome/updater/update_service_internal_impl_qualifying.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"

namespace updater {

bool IsInternalService() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             kServerServiceSwitch) == kServerUpdateServiceInternalSwitchValue;
}

AppServer::AppServer() = default;
AppServer::~AppServer() = default;

int AppServer::Initialize() {
  first_task_ = ModeCheck();
  return kErrorOk;
}

base::OnceClosure AppServer::ModeCheck() {
  VLOG(2) << __func__;
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(updater_scope());
  if (!global_prefs) {
    return base::BindOnce(&AppServer::Shutdown, this,
                          kErrorFailedToLockPrefsMutex);
  }

  const base::Version this_version(kUpdaterVersion);
  base::Version active_version(global_prefs->GetActiveVersion());
  if (!active_version.IsValid()) {
    active_version = base::Version(std::vector<uint32_t>{0});
  }

  VLOG(2) << "This version: " << this_version.GetString()
          << ", active version: " << active_version.GetString();

  if (this_version < active_version) {
    global_prefs = nullptr;
    if (IsInternalService()) {
      uninstall_self_ = true;
      return base::BindOnce(&AppServer::ActiveDutyInternal, this,
                            MakeInactiveUpdateServiceInternal());
    }

#if BUILDFLAG(IS_WIN)
    return base::BindOnce(&AppServer::Shutdown, this,
                          static_cast<int>(UpdateService::Result::kInactive));
#else
    return base::BindOnce(&AppServer::ActiveDuty, this,
                          MakeInactiveUpdateService());
#endif
  }

  if (active_version != base::Version("0") && this_version > active_version) {
    scoped_refptr<LocalPrefs> local_prefs = CreateLocalPrefs(updater_scope());
    if (!local_prefs->GetQualified()) {
      global_prefs = nullptr;
      prefs_ = local_prefs;
      config_ = base::MakeRefCounted<Configurator>(prefs_, external_constants_);
      if (IsInternalService()) {
        return base::BindOnce(
            &AppServer::ActiveDutyInternal, this,
            MakeQualifyingUpdateServiceInternal(config_, local_prefs));
      }

#if BUILDFLAG(IS_WIN)
      return base::BindOnce(&AppServer::Shutdown, this,
                            static_cast<int>(UpdateService::Result::kInactive));
#else
      return base::BindOnce(&AppServer::ActiveDuty, this,
                            MakeInactiveUpdateService());
#endif
    }
  }

  if (this_version > active_version || global_prefs->GetSwapping()) {
    if (!SwapVersions(global_prefs.get(), CreateLocalPrefs(updater_scope()))) {
      return base::BindOnce(&AppServer::Shutdown, this, kErrorFailedToSwap);
    }
  }

  CHECK_EQ(base::Version(global_prefs->GetActiveVersion()),
           base::Version(kUpdaterVersion));

  RepairUpdater(updater_scope(), IsInternalService());

  if (IsInternalService()) {
    prefs_ = CreateLocalPrefs(updater_scope());
    return base::BindOnce(&AppServer::ActiveDutyInternal, this,
                          base::MakeRefCounted<UpdateServiceInternalImpl>());
  }

  server_starts_ = global_prefs->CountServerStarts();
  prefs_ = global_prefs;
  config_ = base::MakeRefCounted<Configurator>(prefs_, external_constants_);
  return base::BindOnce(
      &AppServer::ActiveDuty, this,
      base::MakeRefCounted<UpdateServiceImpl>(updater_scope(), config_));
}

void AppServer::TaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++tasks_running_;
}

void AppServer::TaskCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AppServer> server) {
            --(server->tasks_running_);
            server->OnDelayedTaskComplete();
            if (server->IsIdle() && server->ShutdownIfIdleAfterTask()) {
              server->Shutdown(0);
            }
          },
          base::WrapRefCounted(this)),
      external_constants()->ServerKeepAliveTime());
}

bool AppServer::IsIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tasks_running_ == 0;
}

void AppServer::Uninitialize() {
  // Simply stopping the timer does not destroy its task. The task holds a
  // refcount to this AppServer; therefore the task must be replaced and then
  // the timer stopped.
  hang_timer_.Start(FROM_HERE, base::Minutes(1), base::DoNothing());
  hang_timer_.Stop();
  if (prefs_) {
    PrefsCommitPendingWrites(prefs_->GetPrefService());
  }
  if (uninstall_self_) {
    VLOG(1) << "Uninstalling version " << kUpdaterVersion;
    UninstallSelf();
  } else {
    MaybeUninstall();
  }

  // Because this instance is leaky when running on Windows, the following
  // references must be reset to destroy the objects, otherwise `Prefs` leaks.
  prefs_ = nullptr;
  config_ = nullptr;
}

void AppServer::MaybeUninstall() {
  if (!config_ || IsInternalService()) {
    return;
  }

  scoped_refptr<PersistedData> persisted_data =
      config_->GetUpdaterPersistedData();
  if (ShouldUninstall(persisted_data->GetAppIds(), server_starts_,
                      persisted_data->GetHadApps())) {
    std::optional<base::FilePath> executable =
        GetUpdaterExecutablePath(updater_scope());
    if (executable) {
      base::CommandLine command_line(*executable);
      command_line.AppendSwitch(kUninstallIfUnusedSwitch);
      if (IsSystemInstall(updater_scope())) {
        command_line.AppendSwitch(kSystemSwitch);
      }
      VLOG(2) << "Launching uninstall command: "
              << command_line.GetCommandLineString();

      base::Process process = base::LaunchProcess(command_line, {});
      if (!process.IsValid()) {
        VLOG(2) << "Invalid process launching command: "
                << command_line.GetCommandLineString();
      }
    }
  }
}

void AppServer::FirstTaskRun() {
  std::move(first_task_).Run();
  hang_timer_.Start(FROM_HERE, external_constants_->IdleCheckPeriod(),
                    base::BindRepeating(
                        [](scoped_refptr<AppServer> server) {
                          if (server->IsIdle()) {
                            server->Shutdown(kErrorIdle);
                          }
                        },
                        base::WrapRefCounted(this)));
}

bool AppServer::SwapVersions(GlobalPrefs* global_prefs,
                             scoped_refptr<LocalPrefs> local_prefs) {
  global_prefs->SetSwapping(true);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  if (!global_prefs->GetMigratedLegacyUpdaters()) {
    if (!MigrateLegacyUpdaters(
            updater_scope(),
            base::BindRepeating(
                &PersistedData::RegisterApp,
                base::MakeRefCounted<PersistedData>(
                    updater_scope(), global_prefs->GetPrefService(),
                    std::make_unique<ActivityDataService>(updater_scope()))))) {
      return false;
    }
    global_prefs->SetMigratedLegacyUpdaters();
  }
  if (!SwapInNewVersion()) {
    return false;
  }
  global_prefs->SetActiveVersion(kUpdaterVersion);
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  // Clear the qualified bit: if ActiveVersion downgrades, something has gone
  // wrong and this instance should double-check its qualification before taking
  // back over.
  local_prefs->SetQualified(false);
  PrefsCommitPendingWrites(local_prefs->GetPrefService());
  return true;
}

}  // namespace updater
