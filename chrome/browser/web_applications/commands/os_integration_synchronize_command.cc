// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

namespace {

base::Value SynchronizeOptionsDebugValue(const SynchronizeOsOptions& options) {
  base::Value::Dict debug_dict;
  debug_dict.Set("force_unregister_os_integration",
                 options.force_unregister_os_integration);
  debug_dict.Set("add_shortcut_to_desktop", options.add_shortcut_to_desktop);
  debug_dict.Set("add_to_quick_launch_bar", options.add_to_quick_launch_bar);
  debug_dict.Set("force_create_shortcuts", options.force_create_shortcuts);
  if (options.reason == SHORTCUT_CREATION_AUTOMATED) {
    debug_dict.Set("reason", "SHORTCUT_CREATION_AUTOMATED");
  } else {
    debug_dict.Set("reason", "SHORTCUT_CREATION_BY_USER");
  }
  return base::Value(std::move(debug_dict));
}

}  // namespace

OsIntegrationSynchronizeCommand::OsIntegrationSynchronizeCommand(
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> synchronize_options,
    bool upgrade_to_fully_installed_if_installed,
    base::OnceClosure synchronize_callback)
    : WebAppCommand<AppLock>("OsIntegrationSynchronizeCommand",
                             AppLockDescription(app_id),
                             std::move(synchronize_callback)),
      app_id_(app_id),
      synchronize_options_(synchronize_options),
      upgrade_to_fully_installed_if_installed_(
          upgrade_to_fully_installed_if_installed) {
  GetMutableDebugValue().Set("app_id", app_id_);
  if (synchronize_options_.has_value()) {
    GetMutableDebugValue().Set(
        "synchronize_options",
        SynchronizeOptionsDebugValue(synchronize_options_.value()));
  }
  GetMutableDebugValue().Set("upgrade_to_fully_installed_if_installed",
                             upgrade_to_fully_installed_if_installed);
}

OsIntegrationSynchronizeCommand::~OsIntegrationSynchronizeCommand() = default;

void OsIntegrationSynchronizeCommand::StartWithLock(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  // The app may not be installed, as this command can be scheduled from the
  // command line uninstalling an app from the operating system integration. In
  // that case, `Synchronize` still needs to be called below to attempt cleanup.

  bool in_registrar = !app_lock_->registrar().IsNotInRegistrar(app_id_);
  bool was_installed_with_os_integration =
      app_lock_->registrar().IsInstallState(
          app_id_, {proto::INSTALLED_WITH_OS_INTEGRATION});
  GetMutableDebugValue().Set("in_registrar", in_registrar);
  GetMutableDebugValue().Set("was_fully_installed",
                             was_installed_with_os_integration);

  if (in_registrar && !was_installed_with_os_integration &&
      upgrade_to_fully_installed_if_installed_) {
    ScopedRegistryUpdate update = app_lock_->sync_bridge().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id_);
    CHECK(app);
    app->SetInstallState(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  }

  app_lock_->os_integration_manager().Synchronize(
      app_id_,
      base::BindOnce(&OsIntegrationSynchronizeCommand::OnSynchronizeComplete,
                     weak_factory_.GetWeakPtr()),
      synchronize_options_);
}

void OsIntegrationSynchronizeCommand::OnSynchronizeComplete() {
  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app
