// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/uninstall_all_user_installed_web_apps_command.h"

#include "base/check.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/base/data_type.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {
namespace {
std::string TypesToString(const WebAppManagementTypes& types) {
  std::vector<std::string> types_str;
  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    if (types.Has(type)) {
      types_str.push_back(base::ToString(type));
    }
  }
  return base::JoinString(types_str, ",");
}
std::optional<std::string> ConstructErrorMessage(
    const std::vector<std::string>& errors) {
  std::optional<std::string> error_message = std::nullopt;
  if (!errors.empty()) {
    error_message = base::JoinString(errors, "\n");
  }
  return error_message;
}
}  // namespace

UninstallAllUserInstalledWebAppsCommand::
    UninstallAllUserInstalledWebAppsCommand(
        webapps::WebappUninstallSource uninstall_source,
        Profile& profile,
        Callback callback)
    : WebAppCommand<AllAppsLock, const std::optional<std::string>&>(
          "UninstallAllUserInstalledWebAppsCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/"System shutdown"),
      uninstall_source_(uninstall_source),
      profile_(profile) {
  GetMutableDebugValue().Set("uninstall_source",
                             base::ToString(uninstall_source));
}

UninstallAllUserInstalledWebAppsCommand::
    ~UninstallAllUserInstalledWebAppsCommand() = default;

void UninstallAllUserInstalledWebAppsCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);
  auto& registrar = lock_->registrar();
  for (const web_app::WebApp& app : registrar.GetApps()) {
    if (app.WasInstalledByUser()) {
      ids_to_uninstall_.push_back(app.app_id());
    }
  }
  ProcessNextUninstallOrComplete();
}

void UninstallAllUserInstalledWebAppsCommand::ProcessNextUninstallOrComplete() {
  // All pending jobs and app IDs are finished.
  if (ids_to_uninstall_.empty()) {
    CompleteAndSelfDestruct(
        errors_.empty() ? CommandResult::kSuccess : CommandResult::kFailure,
        ConstructErrorMessage(errors_));
    return;
  }

  // Prepare pending jobs for next app ID.
  webapps::AppId app_id = ids_to_uninstall_.back();
  ids_to_uninstall_.pop_back();

  WebAppManagementTypes types_to_remove =
      base::Intersection(kUserDrivenInstallSources,
                         lock_->registrar().GetAppById(app_id)->GetSources());
  active_job_ = std::make_unique<RemoveInstallSourceJob>(
      uninstall_source_, *profile_, *GetMutableDebugValue().EnsureDict(app_id),
      app_id, types_to_remove);

  active_job_->Start(
      *lock_,
      base::BindOnce(&UninstallAllUserInstalledWebAppsCommand::JobComplete,
                     weak_factory_.GetWeakPtr(), types_to_remove));
}

void UninstallAllUserInstalledWebAppsCommand::JobComplete(
    WebAppManagementTypes types,
    webapps::UninstallResultCode code) {
  CHECK(active_job_);

  if (!webapps::UninstallSucceeded(code)) {
    std::string error_message =
        base::StrCat({active_job_->app_id(), "[", TypesToString(types),
                      "]: ", base::ToString(code)});
    errors_.push_back(error_message);
    GetMutableDebugValue().EnsureList("errors")->Append(error_message);
  }

  active_job_.reset();
  ProcessNextUninstallOrComplete();
}

}  // namespace web_app
