// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"

#include "base/containers/contains.h"
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
  kRemoveInstallSources,
  kRemoveApp,
};

Action GetAction(const WebAppManagementTypes& sources,
                 const WebAppManagementTypes& sources_to_remove) {
  if (sources.empty()) {
    // TODO(crbug.com/40261748): Return a different UninstallResultCode
    // for this case and log it in metrics.
    return Action::kRemoveApp;
  }

  if (!sources.HasAny(sources_to_remove)) {
    return Action::kNone;
  }

  if (sources_to_remove.HasAll(sources)) {
    return Action::kRemoveApp;
  }

  return Action::kRemoveInstallSources;
}

}  // namespace

RemoveInstallSourceJob::RemoveInstallSourceJob(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    base::Value::Dict& debug_value,
    webapps::AppId app_id,
    WebAppManagementTypes install_managements_to_remove)
    : uninstall_source_(uninstall_source),
      profile_(profile),
      debug_value_(debug_value),
      app_id_(app_id),
      install_managements_to_remove_(install_managements_to_remove) {
  debug_value_->Set("!job", "RemoveInstallSourceJob");
  debug_value_->Set("app_id", app_id_);
  debug_value_->Set("uninstall_source", base::ToString(uninstall_source_));
  debug_value_->Set("install_managements_to_remove",
                    base::ToString(install_managements_to_remove_));
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

  switch (GetAction(app->GetSources(), install_managements_to_remove_)) {
    case Action::kNone:
      // TODO(crbug.com/40261748): Return a different UninstallResultCode
      // for when no action is taken instead of being overly specific to the "no
      // app" case.
      CompleteAndSelfDestruct(webapps::UninstallResultCode::kNoAppToUninstall);
      return;

    case Action::kRemoveInstallSources:
      // Removing an install source from an app might change the OS integration
      // characteristics of an app, for example, an app that was installed by
      // both policy and default sources cannot be removed from the OS by an end
      // user on Windows, but if the policy source is removed, the app becomes
      // user uninstallable. This requires an OS integration synchronization to
      // be triggered.
      RemoveInstallSourceFromDatabaseSyncOsIntegration();
      return;

    case Action::kRemoveApp:
      sub_job_ = std::make_unique<RemoveWebAppJob>(
          uninstall_source_, profile_.get(),
          *debug_value_->EnsureDict("sub_job"), app_id_);
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

void RemoveInstallSourceJob::
    RemoveInstallSourceFromDatabaseSyncOsIntegration() {
  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id_);
    for (WebAppManagement::Type source : install_managements_to_remove_) {
      app->RemoveSource(source);
      if (source == WebAppManagement::kSubApp) {
        app->SetParentAppId(std::nullopt);
      }
    }
    WebApp::ExternalConfigMap modified_config_map;
    for (const auto& [type, config_data] :
         app->management_to_external_config_map()) {
      if (!base::Contains(install_managements_to_remove_, type)) {
        modified_config_map.insert_or_assign(type, config_data);
      }
    }
    app->SetWebAppManagementExternalConfigMap(std::move(modified_config_map));
    // TODO(crbug.com/40913556): Make sync uninstall not synchronously
    // remove its sync install source even while a command has an app lock so
    // that we can CHECK(app->HasAnySources()) here.
  }

  lock_->install_manager().NotifyWebAppSourceRemoved(app_id_);
  lock_->os_integration_manager().Synchronize(
      app_id_,
      base::BindOnce(&RemoveInstallSourceJob::CompleteAndSelfDestruct,
                     weak_ptr_factory_.GetWeakPtr(),
                     webapps::UninstallResultCode::kInstallSourceRemoved));
}

void RemoveInstallSourceJob::CompleteAndSelfDestruct(
    webapps::UninstallResultCode code) {
  CHECK(callback_);
  debug_value_->Set("result", base::ToString(code));
  std::move(callback_).Run(code);
}

}  // namespace web_app
