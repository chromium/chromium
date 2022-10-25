// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/update_file_handler_command.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

// static
std::unique_ptr<UpdateFileHandlerCommand>
UpdateFileHandlerCommand::CreateForPersistUserChoice(
    const AppId& app_id,
    bool allowed,
    base::OnceClosure callback,
    WebAppRegistrar* registrar,
    WebAppSyncBridge* sync_bridge,
    OsIntegrationManager* os_integration_manager) {
  return base::WrapUnique(new UpdateFileHandlerCommand(
      app_id, allowed, std::move(callback), registrar, sync_bridge,
      os_integration_manager));
}

// static
std::unique_ptr<UpdateFileHandlerCommand>
UpdateFileHandlerCommand::CreateForUpdate(
    const AppId& app_id,
    base::OnceClosure callback,
    WebAppRegistrar* registrar,
    WebAppSyncBridge* sync_bridge,
    OsIntegrationManager* os_integration_manager) {
  return base::WrapUnique(new UpdateFileHandlerCommand(
      app_id, /*user_choice_to_remember=*/absl::nullopt, std::move(callback),
      registrar, sync_bridge, os_integration_manager));
}

UpdateFileHandlerCommand::UpdateFileHandlerCommand(
    const AppId& app_id,
    absl::optional<bool> user_choice_to_remember,
    base::OnceClosure callback,
    WebAppRegistrar* registrar,
    WebAppSyncBridge* sync_bridge,
    OsIntegrationManager* os_integration_manager)
    : lock_description_(
          std::make_unique<AppLockDescription, base::flat_set<AppId>>(
              {app_id})),
      app_id_(app_id),
      user_choice_to_remember_(std::move(user_choice_to_remember)),
      callback_(std::move(callback)),
      registrar_(registrar),
      sync_bridge_(sync_bridge),
      os_integration_manager_(os_integration_manager) {
  debug_info_.Set("name", "UpdateFileHandlerCommand");
  debug_info_.Set("app_id", app_id_);
  if (user_choice_to_remember_)
    debug_info_.Set("user_choice_to_remember",
                    user_choice_to_remember_.value() ? "allow" : "disallow");
}

UpdateFileHandlerCommand::~UpdateFileHandlerCommand() = default;

void UpdateFileHandlerCommand::Start() {
  debug_info_.Set("was_update_required", false);

  if (!registrar_->IsLocallyInstalled(app_id_)) {
    ReportResultAndDestroy(CommandResult::kFailure);
    return;
  }

  if (user_choice_to_remember_) {
    sync_bridge_->SetAppFileHandlerApprovalState(
        app_id_, user_choice_to_remember_.value()
                     ? ApiApprovalState::kAllowed
                     : ApiApprovalState::kDisallowed);
  }

  // File handling could have been disabled via origin trial as well as user
  // choice, so check both here.
  bool file_handling_enabled =
      os_integration_manager_->IsFileHandlingAPIAvailable(app_id_) &&
      !registrar_->IsAppFileHandlerPermissionBlocked(app_id_);

  // This checks whether the current enabled state matches what we expect
  // to be registered with the OS. If so, no need to do any update.
  if (file_handling_enabled ==
      registrar_->ExpectThatFileHandlersAreRegisteredWithOs(app_id_)) {
    ReportResultAndDestroy(CommandResult::kSuccess);
    return;
  }

  FileHandlerUpdateAction action = file_handling_enabled
                                       ? FileHandlerUpdateAction::kUpdate
                                       : FileHandlerUpdateAction::kRemove;

  debug_info_.Set("was_update_required", true);

#if BUILDFLAG(IS_MAC)
  // On Mac, the file handlers are encoded in the app shortcut. First
  // unregister the file handlers (verifying that it finishes
  // synchronously), then update the shortcut.
  Result unregister_file_handlers_result = Result::kError;
  os_integration_manager_->UpdateFileHandlers(
      app_id_, action,
      base::BindOnce([](Result* result_out,
                        Result actual_result) { *result_out = actual_result; },
                     &unregister_file_handlers_result));
  DCHECK_EQ(Result::kOk, unregister_file_handlers_result);

  // If we're enabling file handling, yet this app does not have any file
  // handlers there is no need to update the shortcut, as doing so would be a
  // no-op anyway.
  const apps::FileHandlers* handlers = registrar_->GetAppFileHandlers(app_id_);
  if (file_handling_enabled && (!handlers || handlers->empty())) {
    OnFileHandlerUpdated(file_handling_enabled, Result::kOk);
  } else {
    // TODO(https://crbug.com/1374916): get result from UpdateShortcuts.
    os_integration_manager_->UpdateShortcuts(
        app_id_, /*old_name=*/{},
        base::BindOnce(&UpdateFileHandlerCommand::OnFileHandlerUpdated,
                       weak_factory_.GetWeakPtr(), file_handling_enabled,
                       Result::kOk));
  }
#else

  os_integration_manager_->UpdateFileHandlers(
      app_id_, action,
      base::BindOnce(&UpdateFileHandlerCommand::OnFileHandlerUpdated,
                     weak_factory_.GetWeakPtr(), file_handling_enabled));
#endif
}

LockDescription& UpdateFileHandlerCommand::lock_description() const {
  return *lock_description_;
}

base::Value UpdateFileHandlerCommand::ToDebugValue() const {
  return base::Value(debug_info_.Clone());
}

void UpdateFileHandlerCommand::OnFileHandlerUpdated(bool file_handling_enabled,
                                                    Result result) {
  DCHECK_EQ(file_handling_enabled,
            registrar_->ExpectThatFileHandlersAreRegisteredWithOs(app_id_));
  ReportResultAndDestroy(result == Result::kOk ? CommandResult::kSuccess
                                               : CommandResult::kFailure);
}

void UpdateFileHandlerCommand::ReportResultAndDestroy(CommandResult result) {
  DCHECK(!callback_.is_null());
  SignalCompletionAndSelfDestruct(result, base::BindOnce(std::move(callback_)));
}

void UpdateFileHandlerCommand::OnShutdown() {
  ReportResultAndDestroy(CommandResult::kShutdown);
}

}  // namespace web_app
