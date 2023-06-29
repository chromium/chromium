// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/uninstall_all_user_installed_web_apps_command.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

UninstallAllUserInstalledWebAppsCommand::
    UninstallAllUserInstalledWebAppsCommand(
        webapps::WebappUninstallSource uninstall_source,
        Profile& profile,
        Callback callback)
    : WebAppCommandTemplate<AllAppsLock>(
          "UninstallAllUserInstalledWebAppsCommand"),
      lock_description_(std::make_unique<AllAppsLockDescription>()),
      uninstall_source_(uninstall_source),
      profile_(profile),
      callback_(std::move(callback)) {}

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
  // Start next pending job.
  if (!pending_jobs_.empty()) {
    std::swap(active_job_, pending_jobs_.back().first);
    auto install_source = pending_jobs_.back().second;
    pending_jobs_.pop_back();

    active_job_->Start(
        *lock_,
        base::BindOnce(&UninstallAllUserInstalledWebAppsCommand::JobComplete,
                       weak_factory_.GetWeakPtr(), install_source));
    return;
  }

  // All pending jobs and app IDs are finished.
  if (ids_to_uninstall_.empty()) {
    CompleteAndSelfDestruct(errors_.empty() ? CommandResult::kSuccess
                                            : CommandResult::kFailure);
    return;
  }

  // Prepare pending jobs for next app ID.
  AppId app_id = ids_to_uninstall_.back();
  ids_to_uninstall_.pop_back();

  for (auto install_source : kUserDrivenInstallSources) {
    pending_jobs_.emplace_back(
        std::make_unique<RemoveInstallSourceJob>(uninstall_source_, *profile_,
                                                 app_id, install_source),
        install_source);
  }

  ProcessNextUninstallOrComplete();
}

void UninstallAllUserInstalledWebAppsCommand::JobComplete(
    WebAppManagement::Type install_source,
    webapps::UninstallResultCode code) {
  CHECK(active_job_);

  debug_info_.EnsureDict(active_job_->app_id())
      ->Set(base::ToString(install_source),
            ConvertUninstallResultCodeToString(code));

  if (code != webapps::UninstallResultCode::kSuccess &&
      code != webapps::UninstallResultCode::kNoAppToUninstall) {
    errors_.push_back(base::StrCat(
        {active_job_->app_id(), "[", base::ToString(install_source),
         "]: ", ConvertUninstallResultCodeToString(code)}));
  }

  active_job_.reset();
  ProcessNextUninstallOrComplete();
}

void UninstallAllUserInstalledWebAppsCommand::OnShutdown() {
  CompleteAndSelfDestruct(CommandResult::kShutdown);
}

const LockDescription&
UninstallAllUserInstalledWebAppsCommand::lock_description() const {
  return *lock_description_;
}

base::Value UninstallAllUserInstalledWebAppsCommand::ToDebugValue() const {
  return base::Value(debug_info_.Clone());
}

void UninstallAllUserInstalledWebAppsCommand::CompleteAndSelfDestruct(
    CommandResult result) {
  CHECK(callback_);
  absl::optional<std::string> error_message = absl::nullopt;
  if (!errors_.empty()) {
    error_message = base::JoinString(errors_, "\n");
  }
  SignalCompletionAndSelfDestruct(
      result, base::BindOnce(std::move(callback_), error_message));
}

}  // namespace web_app
