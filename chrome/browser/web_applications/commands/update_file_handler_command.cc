// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/update_file_handler_command.h"

#include <memory>
#include <utility>

#include "base/barrier_callback.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

// static
std::unique_ptr<UpdateFileHandlerCommand>
UpdateFileHandlerCommand::CreateForPersistUserChoice(
    const webapps::AppId& app_id,
    bool allowed,
    base::OnceClosure callback) {
  return base::WrapUnique(
      new UpdateFileHandlerCommand(app_id, allowed, std::move(callback)));
}

UpdateFileHandlerCommand::UpdateFileHandlerCommand(const webapps::AppId& app_id,
                                                   bool user_choice_to_remember,
                                                   base::OnceClosure callback)
    : WebAppCommand<AppLock>("UpdateFileHandlerCommand",
                             AppLockDescription(app_id),
                             std::move(callback)),
      app_id_(app_id),
      user_choice_to_remember_(user_choice_to_remember) {
  GetMutableDebugValue().Set("app_id", app_id_);
  GetMutableDebugValue().Set("user_choice_to_remember",
                             user_choice_to_remember_ ? "allow" : "disallow");
}

UpdateFileHandlerCommand::~UpdateFileHandlerCommand() = default;

void UpdateFileHandlerCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  GetMutableDebugValue().Set("was_update_required", false);

  lock_ = std::move(lock);

  if (!lock_->registrar().IsInstallState(
          app_id_, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                    proto::INSTALLED_WITH_OS_INTEGRATION})) {
    CompleteAndSelfDestruct(CommandResult::kFailure);
    return;
  }

  lock_->sync_bridge().SetAppFileHandlerApprovalState(
      app_id_, user_choice_to_remember_ ? ApiApprovalState::kAllowed
                                        : ApiApprovalState::kDisallowed);

  // File handling could have been disabled via origin trial as well as user
  // choice, so check both here.
  bool file_handling_enabled =
      !lock_->registrar().IsAppFileHandlerPermissionBlocked(app_id_);

  // This checks whether the current enabled state matches what we expect
  // to be registered with the OS. If so, no need to do any update.
  if (file_handling_enabled ==
      lock_->registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id_)) {
    CompleteAndSelfDestruct(CommandResult::kSuccess);
    return;
  }

  GetMutableDebugValue().Set("was_update_required", true);

  base::OnceClosure file_handler_update_callback =
      base::BindOnce(&UpdateFileHandlerCommand::OnFileHandlerUpdated,
                     weak_factory_.GetWeakPtr(), file_handling_enabled);
  lock_->os_integration_manager().Synchronize(
      app_id_, std::move(file_handler_update_callback));
}

void UpdateFileHandlerCommand::OnFileHandlerUpdated(
    bool file_handling_enabled) {
  DCHECK_EQ(
      file_handling_enabled,
      lock_->registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id_));
  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app
