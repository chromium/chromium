// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"

#include <initializer_list>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

// static
std::unique_ptr<RunOnOsLoginCommand> RunOnOsLoginCommand::CreateForSetLoginMode(
    const webapps::AppId& app_id,
    RunOnOsLoginMode login_mode,
    base::OnceClosure callback) {
  return base::WrapUnique(new RunOnOsLoginCommand(
      app_id, login_mode, RunOnOsLoginAction::kSetModeInDBAndOS,
      std::move(callback)));
}

// static
std::unique_ptr<RunOnOsLoginCommand>
RunOnOsLoginCommand::CreateForSyncLoginMode(const webapps::AppId& app_id,
                                            base::OnceClosure callback) {
  return base::WrapUnique(new RunOnOsLoginCommand(
      app_id,
      /*login_mode=*/absl::nullopt, RunOnOsLoginAction::kSyncModeFromDBToOS,
      std::move(callback)));
}

RunOnOsLoginCommand::RunOnOsLoginCommand(
    webapps::AppId app_id,
    absl::optional<RunOnOsLoginMode> login_mode,
    RunOnOsLoginAction set_or_sync_mode,
    base::OnceClosure callback)
    : WebAppCommandTemplate<AppLock>("RunOnOsLoginCommand"),
      lock_description_(std::make_unique<AppLockDescription>(app_id)),
      app_id_(app_id),
      login_mode_(login_mode),
      set_or_sync_mode_(set_or_sync_mode),
      callback_(std::move(callback)) {}

RunOnOsLoginCommand::~RunOnOsLoginCommand() = default;

const LockDescription& RunOnOsLoginCommand::lock_description() const {
  return *lock_description_;
}

void RunOnOsLoginCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  switch (set_or_sync_mode_) {
    case RunOnOsLoginAction::kSetModeInDBAndOS:
      SetRunOnOsLoginMode();
      break;
    case RunOnOsLoginAction::kSyncModeFromDBToOS:
      SyncRunOnOsLoginMode();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void RunOnOsLoginCommand::OnShutdown() {
  Abort(RunOnOsLoginCommandCompletionState::kCommandSystemShutDown);
  return;
}

base::Value RunOnOsLoginCommand::ToDebugValue() const {
  base::Value::Dict rool_info;
  rool_info.Set("app_id: ", app_id_);
  switch (set_or_sync_mode_) {
    case RunOnOsLoginAction::kSetModeInDBAndOS:
      rool_info.Set("Type of Action: ", "Setting value in DB & OS");
      break;
    case RunOnOsLoginAction::kSyncModeFromDBToOS:
      rool_info.Set("Type of Action: ", "Syncing value in OS from DB");
      break;
    default:
      NOTREACHED();
      break;
  }
  if (!stop_reason_.empty())
    rool_info.Set("Command Stop Reason: ", stop_reason_);
  return base::Value(std::move(rool_info));
}

void RunOnOsLoginCommand::Abort(
    RunOnOsLoginCommandCompletionState aborted_state) {
  if (!callback_)
    return;
  RecordCompletionState(aborted_state);
  switch (aborted_state) {
    case RunOnOsLoginCommandCompletionState::kCommandSystemShutDown:
      stop_reason_ = "Commands System was shut down";
      break;
    case RunOnOsLoginCommandCompletionState::kNotAllowedByPolicy:
      stop_reason_ = "Setting of run on OS login mode not allowed by policy";
      break;
    case RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled:
      stop_reason_ = "App is not locally installed";
      break;
    case RunOnOsLoginCommandCompletionState::kOSHooksNotProperlySet:
      stop_reason_ = "OS Hooks were not properly set";
      break;
    default:
      NOTREACHED();
  }
  SignalCompletionAndSelfDestruct(CommandResult::kFailure,
                                  std::move(callback_));
}

void RunOnOsLoginCommand::SetRunOnOsLoginMode() {
  if (!lock_->registrar().IsLocallyInstalled(app_id_)) {
    Abort(RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled);
    return;
  }

  const auto current_mode = lock_->registrar().GetAppRunOnOsLoginMode(app_id_);

  // Early return if policy does not allow the user to change value, or if the
  // new value is the same as the old value.
  if (!current_mode.user_controllable) {
    Abort(RunOnOsLoginCommandCompletionState::kNotAllowedByPolicy);
    return;
  }

  if (login_mode_.value() == current_mode.value) {
    RecordCompletionState(
        RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched);
    OnOsHooksSet(OsHooksErrors());
    return;
  }

  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    update->UpdateApp(app_id_)->SetRunOnOsLoginMode(login_mode_.value());
  }
  lock_->registrar().NotifyWebAppRunOnOsLoginModeChanged(app_id_,
                                                         login_mode_.value());

  auto synchronize_barrier =
      OsIntegrationManager::GetBarrierForSynchronize(base::BindOnce(
          &RunOnOsLoginCommand::OnOsHooksSet, weak_factory_.GetWeakPtr()));
  lock_->os_integration_manager().Synchronize(
      app_id_, base::BindOnce(synchronize_barrier, OsHooksErrors()));
  UpdateRunOnOsLoginModeWithOsIntegration(synchronize_barrier);
}

void RunOnOsLoginCommand::SyncRunOnOsLoginMode() {
  if (!lock_->registrar().IsLocallyInstalled(app_id_)) {
    Abort(RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled);
    return;
  }
  login_mode_ = lock_->registrar().GetAppRunOnOsLoginMode(app_id_).value;

  if (AreSubManagersExecuteEnabled()) {
    // This is temporary solution for preinstalled apps getting fully installed.
    // Do not run the below 'synchronize' code at all if the expected state ==
    // the desired state.
    // TODO(dmurph): Remove this after 'locally installed without os
    // integration' is implemented for preinstalled apps.
    // https://crbug.com/1480068
    absl::optional<RunOnOsLoginMode> os_integration_state =
        lock_->registrar().GetExpectedRunOnOsLoginOsIntegrationState(app_id_);
    if (os_integration_state && login_mode_.value() == *os_integration_state) {
      RecordCompletionState(
          RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched);
      OnOsHooksSet(OsHooksErrors());
      return;
    }
  }
  auto synchronize_barrier =
      OsIntegrationManager::GetBarrierForSynchronize(base::BindOnce(
          &RunOnOsLoginCommand::OnOsHooksSet, weak_factory_.GetWeakPtr()));
  lock_->os_integration_manager().Synchronize(
      app_id_, base::BindOnce(synchronize_barrier, OsHooksErrors()));
  UpdateRunOnOsLoginModeWithOsIntegration(synchronize_barrier);
}

void RunOnOsLoginCommand::UpdateRunOnOsLoginModeWithOsIntegration(
    base::RepeatingCallback<void(OsHooksErrors)> os_hooks_callback) {
  absl::optional<RunOnOsLoginMode> os_integration_state =
      lock_->registrar().GetExpectedRunOnOsLoginOsIntegrationState(app_id_);

  if (os_integration_state && login_mode_.value() == *os_integration_state) {
    RecordCompletionState(
        RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched);
    std::move(os_hooks_callback).Run(OsHooksErrors());
    return;
  }

  // TODO(crbug.com/1401125): Remove InstallOsHooks() and UninstallOsHooks()
  // once OS integration
  // sub managers have been implemented.
  if (login_mode_.value() == RunOnOsLoginMode::kNotRun) {
    OsHooksOptions os_hooks;
    os_hooks[OsHookType::kRunOnOsLogin] = true;
    lock_->os_integration_manager().UninstallOsHooks(
        app_id_, os_hooks, std::move(os_hooks_callback));
  } else {
    InstallOsHooksOptions install_options;
    install_options.os_hooks[OsHookType::kRunOnOsLogin] = true;
    install_options.reason = SHORTCUT_CREATION_AUTOMATED;
    lock_->os_integration_manager().InstallOsHooks(
        app_id_, std::move(os_hooks_callback), nullptr,
        std::move(install_options));
  }
}

void RunOnOsLoginCommand::OnOsHooksSet(OsHooksErrors errors) {
  if (errors[OsHookType::kRunOnOsLogin] == true) {
    Abort(RunOnOsLoginCommandCompletionState::kOSHooksNotProperlySet);
    return;
  }

  if (!completion_state_set_) {
    RecordCompletionState(
        RunOnOsLoginCommandCompletionState::kSuccessfulCompletion);

    // This is needed for the temporary fix so that the sub-manager version will
    // also save to the old expected state storage.
    // TODO(dmurph): Remove this after 'locally installed without os
    // integration' is implemented for preinstalled apps.
    // https://crbug.com/1480068.
    // Note: minimized isn't supported yet, and gets turned into kWindowed.
    ScopedRegistryUpdate save_state_to_old_expected_value =
        lock_->sync_bridge().BeginUpdate();
    save_state_to_old_expected_value->UpdateApp(app_id_)
        ->SetRunOnOsLoginOsIntegrationState(login_mode_.value() !=
                                                    RunOnOsLoginMode::kNotRun
                                                ? RunOnOsLoginMode::kWindowed
                                                : RunOnOsLoginMode::kNotRun);
  }

  SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                  std::move(callback_));
}

void RunOnOsLoginCommand::RecordCompletionState(
    RunOnOsLoginCommandCompletionState completed_state) {
  completion_state_set_ = true;
  base::UmaHistogramEnumeration("WebApp.RunOnOsLogin.CommandCompletionState",
                                completed_state);
}

}  // namespace web_app
