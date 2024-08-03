// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/set_user_display_mode_command.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
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

  if (app_lock_->registrar().IsNotInRegistrar(app_id_)) {
    CompleteAndSelfDestruct(CommandResult::kFailure);
    return;
  }

  bool needs_os_integration =
      DoSetDisplayMode(*app_lock_, app_id_, user_display_mode_,
                       /*is_user_action=*/true);
  GetMutableDebugValue().Set("needs_os_integration", needs_os_integration);
  if (needs_os_integration) {
    // TODO(crbug.com/339451551): Remove adding to desktop on linux after the
    // OsIntegrationTestOverride can use the xdg install command to detect
    // install.
    SynchronizeOsOptions options;
#if BUILDFLAG(IS_LINUX)
    options.add_shortcut_to_desktop = true;
#endif
    app_lock_->os_integration_manager().Synchronize(
        app_id_,
        base::BindOnce(&SetUserDisplayModeCommand::OnSynchronizeComplete,
                       weak_factory_.GetWeakPtr()),
        options);
  } else {
    CompleteAndSelfDestruct(CommandResult::kSuccess);
  }
}

// static
bool SetUserDisplayModeCommand::DoSetDisplayMode(
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

  // Normally we can exit early, but we might need to synchronize os
  // integration to fix a current situation below.
  bool display_mode_changing =
      resources.registrar().GetAppUserDisplayMode(app_id) != user_display_mode;

  std::optional<proto::InstallState> old_install_state =
      resources.registrar().GetInstallState(app_id);
  if (!old_install_state) {
    // App is not installed.
    return false;
  }

  bool needs_os_integration_sync =
      user_display_mode != mojom::UserDisplayMode::kBrowser &&
      (old_install_state.value() !=
       proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);

  {
    ScopedRegistryUpdate update = resources.sync_bridge().BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    CHECK(web_app, base::NotFatalUntil::M127);
    if (web_app) {
      web_app->SetUserDisplayMode(user_display_mode);
      if (needs_os_integration_sync) {
        web_app->SetInstallState(
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
      }
    }
  }

  if (display_mode_changing) {
    resources.registrar().NotifyWebAppUserDisplayModeChanged(app_id,
                                                             user_display_mode);
  }

  return needs_os_integration_sync;
}

void SetUserDisplayModeCommand::OnSynchronizeComplete() {
  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app
