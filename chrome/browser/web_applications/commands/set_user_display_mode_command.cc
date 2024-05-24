// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/set_user_display_mode_command.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

SetUserDisplayModeCommand::SetUserDisplayModeCommand(
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode,
    base::OnceClosure synchronize_callback)
    : WebAppCommand<AppLock>("SetUserDisplayModeCommand",
                             AppLockDescription(app_id),
                             std::move(synchronize_callback)),
      app_id_(app_id),
      user_display_mode_(user_display_mode) {
  GetMutableDebugValue().Set("app_id", app_id_);
  GetMutableDebugValue().Set("user_display_mode",
                             base::ToString(user_display_mode));
}

SetUserDisplayModeCommand::~SetUserDisplayModeCommand() = default;

void SetUserDisplayModeCommand::StartWithLock(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  if (!app_lock_->registrar().IsLocallyInstalled(app_id_)) {
    CompleteAndSelfDestruct(CommandResult::kFailure);
    return;
  }

  DoSetDisplayMode(*app_lock_, app_id_, user_display_mode_,
                   /*is_user_action=*/true);
  if (user_display_mode_ != mojom::UserDisplayMode::kBrowser) {
    app_lock_->os_integration_manager().Synchronize(
        app_id_,
        base::BindOnce(&SetUserDisplayModeCommand::OnSynchronizeComplete,
                       weak_factory_.GetWeakPtr()),
        {});
  } else {
    CompleteAndSelfDestruct(CommandResult::kSuccess);
  }
}

// static
void SetUserDisplayModeCommand::DoSetDisplayMode(
    WithAppResources& resources,
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode,
    bool is_user_action) {
  if (is_user_action) {
    switch (user_display_mode) {
      case mojom::UserDisplayMode::kStandalone:
        base::RecordAction(
            base::UserMetricsAction("WebApp.SetWindowMode.Window"));
        break;
      case mojom::UserDisplayMode::kBrowser:
        base::RecordAction(base::UserMetricsAction("WebApp.SetWindowMode.Tab"));
        break;
      case mojom::UserDisplayMode::kTabbed:
        base::RecordAction(
            base::UserMetricsAction("WebApp.SetWindowMode.Tabbed"));
        break;
    }
  }

  {
    ScopedRegistryUpdate update = resources.sync_bridge().BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app) {
      web_app->SetUserDisplayMode(user_display_mode);
    }
  }

  resources.registrar().NotifyWebAppUserDisplayModeChanged(app_id,
                                                           user_display_mode);
}

void SetUserDisplayModeCommand::OnSynchronizeComplete() {
  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app
