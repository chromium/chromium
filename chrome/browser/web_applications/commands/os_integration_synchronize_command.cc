// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
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
    base::OnceClosure synchronize_callback)
    : WebAppCommand<AppLock>("OsIntegrationSynchronizeCommand",
                             AppLockDescription(app_id),
                             std::move(synchronize_callback)),
      app_id_(app_id),
      synchronize_options_(synchronize_options) {
  GetMutableDebugValue().Set("app_id", app_id_);
  if (synchronize_options_.has_value()) {
    GetMutableDebugValue().Set(
        "synchronize_options",
        SynchronizeOptionsDebugValue(synchronize_options_.value()));
  }
}

OsIntegrationSynchronizeCommand::~OsIntegrationSynchronizeCommand() = default;

void OsIntegrationSynchronizeCommand::StartWithLock(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

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
