// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

OsIntegrationSynchronizeCommand::OsIntegrationSynchronizeCommand(
    const AppId& app_id,
    absl::optional<SynchronizeOsOptions> synchronize_options,
    base::OnceClosure synchronize_callback)
    : WebAppCommandTemplate<AppLock>("OsIntegrationSynchronizeCommand"),
      app_lock_description_(std::make_unique<AppLockDescription>(app_id)),
      app_id_(app_id),
      synchronize_options_(synchronize_options),
      synchronize_callback_(std::move(synchronize_callback)) {}

OsIntegrationSynchronizeCommand::~OsIntegrationSynchronizeCommand() = default;

const LockDescription& OsIntegrationSynchronizeCommand::lock_description()
    const {
  return *app_lock_description_;
}

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
  DCHECK(!synchronize_callback_.is_null());
  SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                  std::move(synchronize_callback_));
}

void OsIntegrationSynchronizeCommand::OnSyncSourceRemoved() {}

void OsIntegrationSynchronizeCommand::OnShutdown() {
  DCHECK(!synchronize_callback_.is_null());
  SignalCompletionAndSelfDestruct(CommandResult::kShutdown,
                                  std::move(synchronize_callback_));
}

base::Value OsIntegrationSynchronizeCommand::ToDebugValue() const {
  base::Value::Dict value;
  value.Set("app_id", app_id_);
  return base::Value(std::move(value));
}

}  // namespace web_app
