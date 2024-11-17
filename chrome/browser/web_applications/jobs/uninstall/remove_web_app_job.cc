// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

namespace {

bool IsOsIntegrationRemovedForApp(
    std::optional<proto::WebAppOsIntegrationState> state) {
  if (!state.has_value()) {
    return true;
  }

  return !state->has_file_handling() && !state->has_protocols_handled() &&
         !state->has_run_on_os_login() && !state->has_shortcut() &&
         !state->has_shortcut_menus() && !state->has_uninstall_registration();
}

}  // namespace

RemoveWebAppJob::RemoveWebAppJob(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    base::Value::Dict& debug_value,
    webapps::AppId app_id)
    : uninstall_source_(uninstall_source),
      profile_(profile),
      debug_value_(debug_value),
      app_id_(app_id) {
  base::Value::Dict dict;
  debug_value_->Set("!job", "RemoveWebAppJob");
  debug_value_->Set("app_id", app_id_);
}

RemoveWebAppJob::~RemoveWebAppJob() = default;

void RemoveWebAppJob::Start(AllAppsLock& lock, Callback callback) {
  lock_ = &lock;
  callback_ = std::move(callback);
  debug_value_->Set("has_callback", !callback_.is_null());

  const WebApp* app = lock_->registrar().GetAppById(app_id_);
  if (!app) {
    CompleteAndSelfDestruct(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  if (app->isolation_data().has_value()) {
    has_isolated_storage_ = true;
  }

  if (webapps::IsUserUninstall(uninstall_source_)) {
    CHECK(app->CanUserUninstallWebApp());
    if (app->IsPreinstalledApp()) {
      // Update the default uninstalled web_app prefs if it is a preinstalled
      // app but being removed by user.
      const WebApp::ExternalConfigMap& config_map =
          app->management_to_external_config_map();
      auto it = config_map.find(WebAppManagement::kDefault);
      if (it != config_map.end()) {
        UserUninstalledPreinstalledWebAppPrefs(profile_->GetPrefs())
            .Add(app_id_, it->second.install_urls);
      }
    }
  }

  sub_apps_pending_removal_ = lock_->registrar().GetAllSubAppIds(app_id_);
  base::Value::List* list = debug_value_->EnsureList("sub_apps_found");
  for (const webapps::AppId& sub_app_id : sub_apps_pending_removal_) {
    list->Append(sub_app_id);
  }

  lock_->install_manager().NotifyWebAppWillBeUninstalled(app_id_);

  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    WebApp* mutable_app = update->UpdateApp(app_id_);
    CHECK(mutable_app);
    mutable_app->SetIsUninstalling(true);
  }

  // For Isolated Web App:
  // - Sets pref value to garbage-collect StoragePartitions on next start up.
  // - Clears data on StoragePartitions to prevent data leak on reinstall
  // before GC.
  if (has_isolated_storage_) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kShouldGarbageCollectStoragePartitions, true);

    location_ = app->isolation_data()->location();

    url::Origin iwa_origin = url::Origin::Create(app->scope());
    web_app::RemoveIsolatedWebAppBrowsingData(
        &profile_.get(), iwa_origin,
        base::BindOnce(&RemoveWebAppJob::OnIsolatedWebAppBrowsingDataCleared,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  lock_->os_integration_manager().Synchronize(
      app_id_, base::BindOnce(&RemoveWebAppJob::SynchronizeAndUninstallOsHooks,
                              weak_ptr_factory_.GetWeakPtr()));

  // While sometimes `Synchronize` needs to read icon data, for the uninstall
  // case it never needs to be read. Thus, it is safe to schedule this now and
  // not after the `Synchronize` call completes.
  lock_->icon_manager().DeleteData(
      app_id_, base::BindOnce(&RemoveWebAppJob::OnIconDataDeleted,
                              weak_ptr_factory_.GetWeakPtr()));

  lock_->translation_manager().DeleteTranslations(
      app_id_, base::BindOnce(&RemoveWebAppJob::OnTranslationDataDeleted,
                              weak_ptr_factory_.GetWeakPtr()));

}

webapps::WebappUninstallSource RemoveWebAppJob::uninstall_source() const {
  return uninstall_source_;
}

void RemoveWebAppJob::SynchronizeAndUninstallOsHooks() {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!hooks_uninstalled_);
  hooks_uninstalled_ = true;
  bool os_integration_removal_success = IsOsIntegrationRemovedForApp(
      lock_->registrar().GetAppCurrentOsIntegrationState(app_id_));
  debug_value_->Set("os_integration_removal_success",
                    os_integration_removal_success);
  errors_ = errors_ || !os_integration_removal_success;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::OnIconDataDeleted(bool success) {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!app_data_deleted_);
  app_data_deleted_ = true;
  debug_value_->Set("app_data_deleted_success", success);
  errors_ = errors_ || !success;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::OnTranslationDataDeleted(bool success) {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!translation_data_deleted_);
  translation_data_deleted_ = true;
  debug_value_->Set("translation_data_deleted_success", success);
  errors_ = errors_ || !success;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::OnIsolatedWebAppBrowsingDataCleared() {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!isolated_web_app_browsing_data_cleared_);
  // Must be an Isolated Web App.
  CHECK(has_isolated_storage_);
  CHECK(location_.has_value());
  isolated_web_app_browsing_data_cleared_ = true;
  debug_value_->Set("isolated_web_app_browsing_data_cleared_success", true);

  web_app::CloseAndDeleteBundle(
      &profile_.get(), location_.value(),
      base::BindOnce(&RemoveWebAppJob::OnIsolatedWebAppOwnedLocationDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoveWebAppJob::OnIsolatedWebAppOwnedLocationDeleted() {
  CHECK(!primary_removal_result_.has_value());
  CHECK(has_isolated_storage_);
  CHECK(!isolated_web_app_owned_location_deleted_);

  isolated_web_app_owned_location_deleted_ = true;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::MaybeFinishPrimaryRemoval() {
  CHECK(!primary_removal_result_.has_value());
  const bool is_iwa_fully_removed = isolated_web_app_browsing_data_cleared_ &&
                                    isolated_web_app_owned_location_deleted_;
  if (!hooks_uninstalled_ || !app_data_deleted_ || !translation_data_deleted_ ||
      (has_isolated_storage_ && !is_iwa_fully_removed)) {
    return;
  }

  {
    CHECK_NE(lock_->registrar().GetAppById(app_id_), nullptr);
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    update->DeleteApp(app_id_);
  }

  primary_removal_result_ = errors_ ? webapps::UninstallResultCode::kError
                                    : webapps::UninstallResultCode::kAppRemoved;
  debug_value_->Set("primary_removal_result",
                    base::ToString(primary_removal_result_.value()));
  base::UmaHistogramBoolean("WebApp.Uninstall.Result", !errors_);

  lock_->install_manager().NotifyWebAppUninstalled(app_id_, uninstall_source_);

  ProcessSubAppsPendingRemovalOrComplete();
}

void RemoveWebAppJob::ProcessSubAppsPendingRemovalOrComplete() {
  CHECK(primary_removal_result_.has_value());

  if (sub_job_) {
    sub_job_.reset();
  }

  if (sub_apps_pending_removal_.empty()) {
    CompleteAndSelfDestruct(primary_removal_result_.value());
    return;
  }

  webapps::AppId sub_app_id = std::move(sub_apps_pending_removal_.back());
  sub_apps_pending_removal_.pop_back();

  sub_job_ = std::make_unique<RemoveInstallSourceJob>(
      uninstall_source_, profile_.get(),
      *debug_value_->EnsureDict("sub_app_jobs")->EnsureDict(sub_app_id),
      sub_app_id, WebAppManagementTypes({WebAppManagement::Type::kSubApp}));
  sub_job_->Start(*lock_,
                  base::IgnoreArgs<webapps::UninstallResultCode>(base::BindOnce(
                      &RemoveWebAppJob::ProcessSubAppsPendingRemovalOrComplete,
                      weak_ptr_factory_.GetWeakPtr())));
}

void RemoveWebAppJob::CompleteAndSelfDestruct(
    webapps::UninstallResultCode code) {
  CHECK(callback_);
  debug_value_->Set("result", base::ToString(code));
  std::move(callback_).Run(code);
}

}  // namespace web_app
