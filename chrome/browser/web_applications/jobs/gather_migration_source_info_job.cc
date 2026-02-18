// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/gather_migration_source_info_job.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/web_applications/jobs/gather_migration_source_info_job_result.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

GatherMigrationSourceInfoJob::GatherMigrationSourceInfoJob(
    WithAppResources& lock,
    const webapps::AppId& source_app_id,
    const webapps::AppId& destination_app_id,
    Callback callback)
    : lock_(lock),
      source_app_id_(source_app_id),
      destination_app_id_(destination_app_id),
      callback_(std::move(callback)) {}

GatherMigrationSourceInfoJob::~GatherMigrationSourceInfoJob() = default;

void GatherMigrationSourceInfoJob::Start() {
  const WebApp* source_app = lock_->registrar().GetAppById(source_app_id_);
  if (!source_app ||
      (source_app->install_state() !=
           proto::InstallState::INSTALLED_WITH_OS_INTEGRATION &&
       source_app->install_state() !=
           proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION)) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  result_.install_state = source_app->install_state();
  result_.user_display_mode = source_app->user_display_mode();

  ValueWithPolicy<RunOnOsLoginMode> destination_rool_allowed =
      lock_->registrar().GetAppRunOnOsLoginMode(destination_app_id_);
  if (destination_rool_allowed.user_controllable &&
      source_app->run_on_os_login_mode() != RunOnOsLoginMode::kNotRun) {
    result_.run_on_os_login_mode = source_app->run_on_os_login_mode();
  } else {
    result_.run_on_os_login_mode = RunOnOsLoginMode::kNotRun;
  }

  lock_->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
      source_app_id_,
      base::BindOnce(&GatherMigrationSourceInfoJob::OnShortcutInfoRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GatherMigrationSourceInfoJob::OnShortcutInfoRetrieved(
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (!shortcut_info) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  lock_->os_integration_manager().GetAppExistingShortCutLocation(
      base::BindOnce(&GatherMigrationSourceInfoJob::OnShortcutLocationGathered,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(shortcut_info));
}

void GatherMigrationSourceInfoJob::OnShortcutLocationGathered(
    ShortcutLocations locations) {
  if (locations.in_startup &&
      result_.run_on_os_login_mode == RunOnOsLoginMode::kNotRun) {
    ValueWithPolicy<RunOnOsLoginMode> destination_rool_allowed =
        lock_->registrar().GetAppRunOnOsLoginMode(destination_app_id_);
    if (destination_rool_allowed.user_controllable) {
      result_.run_on_os_login_mode = RunOnOsLoginMode::kWindowed;
    }
  }

  result_.shortcut_locations = locations;
  std::move(callback_).Run(std::move(result_));
}

}  // namespace web_app
