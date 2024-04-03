// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/update_protocol_handler_approval_command.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

UpdateProtocolHandlerApprovalCommand::UpdateProtocolHandlerApprovalCommand(
    const webapps::AppId& app_id,
    const std::string& protocol_scheme,
    ApiApprovalState approval_state,
    base::OnceClosure callback)
    : WebAppCommand<AppLock>("UpdateProtocolHandlerApprovalCommand",
                             AppLockDescription(app_id),
                             std::move(callback)),
      app_id_(app_id),
      protocol_scheme_(protocol_scheme),
      approval_state_(approval_state) {
  GetMutableDebugValue().Set("name", "UpdateProtocolHandlerApprovalCommand");
  GetMutableDebugValue().Set("app_id", app_id_);
  GetMutableDebugValue().Set("api_approval_state",
                             base::ToString(approval_state_));
  GetMutableDebugValue().Set("protocol_scheme", protocol_scheme);
  DCHECK(!protocol_scheme.empty());
}

UpdateProtocolHandlerApprovalCommand::~UpdateProtocolHandlerApprovalCommand() =
    default;

void UpdateProtocolHandlerApprovalCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  OsIntegrationManager& os_integration_manager =
      lock_->os_integration_manager();
  const std::vector<custom_handlers::ProtocolHandler>
      original_protocol_handlers =
          os_integration_manager.GetAppProtocolHandlers(app_id_);

  // Use a scope here, so that the web app registry is updated when
  // `update` goes out of scope. If it doesn't then observers will
  // examine stale data.
  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id_);
    if (!app_to_update) {
      // If this command is scheduled after an uninstallation, the
      // app will no longer exist, in which case we should gracefully terminate
      // the command and run the final callback.
      GetMutableDebugValue().Set("failure_reason", "app_not_found");
      CompleteAndSelfDestruct(CommandResult::kFailure);
      return;
    }

    base::flat_set<std::string> allowed_protocols(
        app_to_update->allowed_launch_protocols());
    base::flat_set<std::string> disallowed_protocols(
        app_to_update->disallowed_launch_protocols());

    switch (approval_state_) {
      case ApiApprovalState::kAllowed:
        allowed_protocols.insert(protocol_scheme_);
        disallowed_protocols.erase(protocol_scheme_);
        break;
      case ApiApprovalState::kDisallowed:
        allowed_protocols.erase(protocol_scheme_);
        disallowed_protocols.insert(protocol_scheme_);
        break;
      case ApiApprovalState::kRequiresPrompt:
        allowed_protocols.erase(protocol_scheme_);
        disallowed_protocols.erase(protocol_scheme_);
        break;
    }

    app_to_update->SetAllowedLaunchProtocols(std::move(allowed_protocols));
    app_to_update->SetDisallowedLaunchProtocols(
        std::move(disallowed_protocols));
  }
  // Notify observers that the list of allowed or disallowed protocols was
  // updated.
  lock_->registrar().NotifyWebAppProtocolSettingsChanged();

  os_integration_manager.Synchronize(
      app_id_,
      base::BindOnce(&UpdateProtocolHandlerApprovalCommand::
                         OnProtocolHandlersSynchronizeComplete,
                     weak_factory_.GetWeakPtr(), original_protocol_handlers,
                     std::ref(os_integration_manager)));
}

void UpdateProtocolHandlerApprovalCommand::
    OnProtocolHandlersSynchronizeComplete(
        const std::vector<custom_handlers::ProtocolHandler>&
            original_protocol_handlers,
        OsIntegrationManager& os_integration_manager) {
  // OS protocol registration does not need to be updated.
  if (original_protocol_handlers ==
      os_integration_manager.GetAppProtocolHandlers(app_id_)) {
    GetMutableDebugValue().Set("was_update_required", false);
  } else {
    GetMutableDebugValue().Set("was_update_required", true);
  }
  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app
