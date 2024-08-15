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
#include "base/notreached.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
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
      /*login_mode=*/std::nullopt, RunOnOsLoginAction::kSyncModeFromDBToOS,
      std::move(callback)));
}

RunOnOsLoginCommand::RunOnOsLoginCommand(
    webapps::AppId app_id,
    std::optional<RunOnOsLoginMode> login_mode,
    RunOnOsLoginAction set_or_sync_mode,
    base::OnceClosure callback)
    : WebAppCommand<AppLock>("RunOnOsLoginCommand",
                             AppLockDescription(app_id),
                             std::move(callback)),
      app_id_(app_id),
      login_mode_(login_mode),
      set_or_sync_mode_(set_or_sync_mode) {
  GetMutableDebugValue().Set("app_id", app_id_);
  switch (set_or_sync_mode_) {
    case RunOnOsLoginAction::kSetModeInDBAndOS:
      GetMutableDebugValue().Set("type_of_action", "set_db_os_value");
      break;
    case RunOnOsLoginAction::kSyncModeFromDBToOS:
      GetMutableDebugValue().Set("type_of_action", "sync_db_os_value");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

RunOnOsLoginCommand::~RunOnOsLoginCommand() = default;

void RunOnOsLoginCommand::OnShutdown(
    base::PassKey<WebAppCommandManager>) const {
  base::UmaHistogramEnumeration(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kCommandSystemShutDown);
}

void RunOnOsLoginCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (!lock_->registrar().IsInstallState(
          app_id_, {proto::INSTALLED_WITH_OS_INTEGRATION,
                    proto::INSTALLED_WITHOUT_OS_INTEGRATION})) {
    Abort(RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled);
    return;
  }

  switch (set_or_sync_mode_) {
    case RunOnOsLoginAction::kSetModeInDBAndOS:
      SetRunOnOsLoginMode();
      break;
    case RunOnOsLoginAction::kSyncModeFromDBToOS:
      SyncRunOnOsLoginMode();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void RunOnOsLoginCommand::Abort(
    RunOnOsLoginCommandCompletionState aborted_state) {
  RecordCompletionState(aborted_state);
  switch (aborted_state) {
    case RunOnOsLoginCommandCompletionState::kCommandSystemShutDown:
      NOTREACHED();
    case RunOnOsLoginCommandCompletionState::kNotAllowedByPolicy:
      stop_reason_ = "Setting of run on OS login mode not allowed by policy";
      break;
    case RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled:
      stop_reason_ = "App is not locally installed";
      break;
    default:
      NOTREACHED();
  }
  GetMutableDebugValue().Set("Command Stop Reason: ", stop_reason_);
  CompleteAndSelfDestruct(CommandResult::kFailure);
}

void RunOnOsLoginCommand::SetRunOnOsLoginMode() {
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
    OnOsIntegrationSynchronized();
    return;
  }

  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    update->UpdateApp(app_id_)->SetRunOnOsLoginMode(login_mode_.value());
  }
  lock_->registrar().NotifyWebAppRunOnOsLoginModeChanged(app_id_,
                                                         login_mode_.value());

  lock_->os_integration_manager().Synchronize(
      app_id_, base::BindOnce(&RunOnOsLoginCommand::OnOsIntegrationSynchronized,
                              weak_factory_.GetWeakPtr()));
}

void RunOnOsLoginCommand::SyncRunOnOsLoginMode() {
  login_mode_ = lock_->registrar().GetAppRunOnOsLoginMode(app_id_).value;


  lock_->os_integration_manager().Synchronize(
      app_id_, base::BindOnce(&RunOnOsLoginCommand::OnOsIntegrationSynchronized,
                              weak_factory_.GetWeakPtr()));
}

void RunOnOsLoginCommand::OnOsIntegrationSynchronized() {
  if (!completion_state_set_) {
    RecordCompletionState(
        RunOnOsLoginCommandCompletionState::kSuccessfulCompletion);
  }

  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

void RunOnOsLoginCommand::RecordCompletionState(
    RunOnOsLoginCommandCompletionState completed_state) {
  completion_state_set_ = true;
  base::UmaHistogramEnumeration("WebApp.RunOnOsLogin.CommandCompletionState",
                                completed_state);
}

}  // namespace web_app
