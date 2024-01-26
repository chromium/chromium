// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"

#include "base/strings/to_string.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

namespace {

enum class Action {
  kNone,
  kRemoveInstallSource,
  kRemoveApp,
};

Action GetAction(const WebAppManagementTypes& sources,
                 WebAppManagement::Type install_source) {
  if (sources.Empty()) {
    // TODO(crbug.com/1427340): Return a different UninstallResultCode
    // for this case and log it in metrics.
    return Action::kRemoveApp;
  }

  if (!sources.Has(install_source)) {
    return Action::kNone;
  }

  if (sources.Size() > 1) {
    return Action::kRemoveInstallSource;
  }

  CHECK_EQ(sources.Size(), 1u);
  CHECK(sources.Has(install_source));
  return Action::kRemoveApp;
}

}  // namespace

RemoveInstallSourceJob::RemoveInstallSourceJob(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    base::Value::Dict& debug_value,
    webapps::AppId app_id,
    WebAppManagement::Type install_source)
    : uninstall_source_(uninstall_source),
      profile_(profile),
      debug_value_(debug_value),
      app_id_(app_id),
      install_source_(install_source) {
  debug_value_->Set("!job", "RemoveInstallSourceJob");
  debug_value_->Set("app_id", app_id_);
  debug_value_->Set("uninstall_source", base::ToString(uninstall_source_));
  debug_value_->Set("install_source", base::ToString(install_source_));
}

RemoveInstallSourceJob::~RemoveInstallSourceJob() = default;

void RemoveInstallSourceJob::Start(AllAppsLock& lock, Callback callback) {
  lock_ = &lock;
  callback_ = std::move(callback);
  debug_value_->Set("has_callback", !callback_.is_null());

  const WebApp* app = lock_->registrar().GetAppById(app_id_);
  if (!app) {
    CompleteAndSelfDestruct(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  WebAppManagement::Type install_source = install_source_;
  switch (GetAction(app->GetSources(), install_source)) {
    case Action::kNone:
      // TODO(crbug.com/1427340): Return a different UninstallResultCode
      // for when no action is taken instead of being overly specific to the "no
      // app" case.
      CompleteAndSelfDestruct(webapps::UninstallResultCode::kNoAppToUninstall);
      return;

    case Action::kRemoveInstallSource:
      // Install sources may block user uninstallation (e.g. policy), if one of
      // these install sources is being removed then the ability to uninstall
      // may need to be re-deployed into the OS.
      MaybeRegisterOsUninstall(
          app, install_source, lock_->os_integration_manager(),
          base::BindOnce(
              &RemoveInstallSourceJob::RemoveInstallSourceFromDatabase,
              weak_ptr_factory_.GetWeakPtr()));
      return;

    case Action::kRemoveApp:
      sub_job_ = std::make_unique<RemoveWebAppJob>(
          uninstall_source_, profile_.get(),
          *debug_value_->EnsureDict("sub_job"), app_id_,
          /*is_initial_request=*/false);
      sub_job_->Start(
          *lock_,
          base::BindOnce(&RemoveInstallSourceJob::CompleteAndSelfDestruct,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
  }
}

webapps::WebappUninstallSource RemoveInstallSourceJob::uninstall_source()
    const {
  return uninstall_source_;
}

void RemoveInstallSourceJob::RemoveInstallSourceFromDatabase(
    OsHooksErrors os_hooks_errors) {
  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id_);
    app->RemoveSource(install_source_);
    if (install_source_ == WebAppManagement::kSubApp) {
      app->SetParentAppId(std::nullopt);
    }
    // TODO(crbug.com/1447308): Make sync uninstall not synchronously
    // remove its sync install source even while a command has an app lock so
    // that we can CHECK(app->HasAnySources()) here.
  }

  lock_->install_manager().NotifyWebAppSourceRemoved(app_id_);

  CompleteAndSelfDestruct(webapps::UninstallResultCode::kSuccess);
}

void RemoveInstallSourceJob::CompleteAndSelfDestruct(
    webapps::UninstallResultCode code) {
  CHECK(callback_);
  debug_value_->Set("result", base::ToString(code));
  std::move(callback_).Run(code);
}

}  // namespace web_app
