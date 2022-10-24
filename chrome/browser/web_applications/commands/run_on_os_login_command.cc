// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"

#include <initializer_list>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

// static
std::unique_ptr<RunOnOsLoginCommand> RunOnOsLoginCommand::CreateForSetLoginMode(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    WebAppSyncBridge* sync_bridge,
    const AppId& app_id,
    RunOnOsLoginMode login_mode,
    base::OnceClosure callback) {
  DCHECK(registrar);
  DCHECK(os_integration_manager);
  DCHECK(sync_bridge);

  return base::WrapUnique(new RunOnOsLoginCommand(
      app_id, registrar, os_integration_manager, sync_bridge, login_mode,
      RunOnOsLoginAction::kSetModeInDBAndOS, std::move(callback)));
}

// static
std::unique_ptr<RunOnOsLoginCommand>
RunOnOsLoginCommand::CreateForSyncLoginMode(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    const AppId& app_id,
    base::OnceClosure callback) {
  DCHECK(registrar);
  DCHECK(os_integration_manager);

  return base::WrapUnique(new RunOnOsLoginCommand(
      app_id, registrar, os_integration_manager, /*sync_bridge=*/nullptr,
      /*login_mode=*/absl::nullopt, RunOnOsLoginAction::kSyncModeFromDBToOS,
      std::move(callback)));
}

RunOnOsLoginCommand::RunOnOsLoginCommand(
    AppId app_id,
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    WebAppSyncBridge* sync_bridge,
    absl::optional<RunOnOsLoginMode> login_mode,
    RunOnOsLoginAction set_or_sync_mode,
    base::OnceClosure callback)
    : lock_description_(
          std::make_unique<AppLockDescription, base::flat_set<AppId>>(
              {app_id})),
      app_id_(app_id),
      registrar_(registrar),
      os_integration_manager_(os_integration_manager),
      sync_bridge_(sync_bridge),
      login_mode_(login_mode),
      set_or_sync_mode_(set_or_sync_mode),
      callback_(std::move(callback)) {}

RunOnOsLoginCommand::~RunOnOsLoginCommand() = default;

LockDescription& RunOnOsLoginCommand::lock_description() const {
  return *lock_description_;
}

void RunOnOsLoginCommand::Start() {
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
  rool_info.Set("RunOnOsLoginCommand ID:", id());
  rool_info.Set("App Id: ", app_id_);
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
  if (!registrar_->IsLocallyInstalled(app_id_)) {
    Abort(RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled);
    return;
  }

  const auto current_mode = registrar_->GetAppRunOnOsLoginMode(app_id_);

  // Early return if policy does not allow the user to change value, or if the
  // new value is the same as the old value.
  if (!current_mode.user_controllable) {
    Abort(RunOnOsLoginCommandCompletionState::kNotAllowedByPolicy);
    return;
  }

  if (login_mode_.value() == current_mode.value) {
    RecordCompletionState(
        RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched);
    SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                    std::move(callback_));
    return;
  }

  {
    ScopedRegistryUpdate update(sync_bridge_);
    update->UpdateApp(app_id_)->SetRunOnOsLoginMode(login_mode_.value());
  }
  registrar_->NotifyWebAppRunOnOsLoginModeChanged(app_id_, login_mode_.value());
  UpdateRunOnOsLoginModeWithOsIntegration();
}

void RunOnOsLoginCommand::SyncRunOnOsLoginMode() {
  if (!registrar_->IsLocallyInstalled(app_id_)) {
    Abort(RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled);
    return;
  }
  login_mode_ = registrar_->GetAppRunOnOsLoginMode(app_id_).value;
  UpdateRunOnOsLoginModeWithOsIntegration();
}

void RunOnOsLoginCommand::UpdateRunOnOsLoginModeWithOsIntegration() {
  absl::optional<RunOnOsLoginMode> os_integration_state =
      registrar_->GetExpectedRunOnOsLoginOsIntegrationState(app_id_);

  if (os_integration_state && login_mode_.value() == *os_integration_state) {
    RecordCompletionState(
        RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched);
    SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                    std::move(callback_));
    return;
  }

  if (login_mode_.value() == RunOnOsLoginMode::kNotRun) {
    web_app::OsHooksOptions os_hooks;
    os_hooks[web_app::OsHookType::kRunOnOsLogin] = true;
    os_integration_manager_->UninstallOsHooks(
        app_id_, os_hooks,
        base::BindOnce(&RunOnOsLoginCommand::OnOsHooksSet,
                       weak_factory_.GetWeakPtr()));
  } else {
    web_app::InstallOsHooksOptions install_options;
    install_options.os_hooks[web_app::OsHookType::kRunOnOsLogin] = true;
    install_options.reason = SHORTCUT_CREATION_AUTOMATED;
    os_integration_manager_->InstallOsHooks(
        app_id_,
        base::BindOnce(&RunOnOsLoginCommand::OnOsHooksSet,
                       weak_factory_.GetWeakPtr()),
        nullptr, std::move(install_options));
  }
}

void RunOnOsLoginCommand::OnOsHooksSet(web_app::OsHooksErrors errors) {
  if (errors[web_app::OsHookType::kRunOnOsLogin] == true) {
    Abort(RunOnOsLoginCommandCompletionState::kOSHooksNotProperlySet);
    return;
  }
  RecordCompletionState(
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion);
  SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                  std::move(callback_));
}

void RunOnOsLoginCommand::RecordCompletionState(
    RunOnOsLoginCommandCompletionState completed_state) {
  base::UmaHistogramEnumeration("WebApp.RunOnOsLogin.CommandCompletionState",
                                completed_state);
}

}  // namespace web_app
