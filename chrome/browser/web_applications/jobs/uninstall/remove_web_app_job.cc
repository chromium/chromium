// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_browsing_data.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/uninstall_result_code.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

bool CanUninstallAllManagementSources(
    webapps::WebappUninstallSource uninstall_source) {
  // Check that the source was from a known 'user' or allowed ones such
  // as kMigration.
  return uninstall_source == webapps::WebappUninstallSource::kUnknown ||
         uninstall_source == webapps::WebappUninstallSource::kAppMenu ||
         uninstall_source == webapps::WebappUninstallSource::kAppsPage ||
         uninstall_source == webapps::WebappUninstallSource::kOsSettings ||
         uninstall_source == webapps::WebappUninstallSource::kAppManagement ||
         uninstall_source == webapps::WebappUninstallSource::kMigration ||
         uninstall_source == webapps::WebappUninstallSource::kAppList ||
         uninstall_source == webapps::WebappUninstallSource::kShelf ||
         uninstall_source == webapps::WebappUninstallSource::kSync ||
         uninstall_source == webapps::WebappUninstallSource::kStartupCleanup ||
         uninstall_source == webapps::WebappUninstallSource::kTestCleanup ||
         uninstall_source ==
             webapps::WebappUninstallSource::kHealthcareUserInstallCleanup;
}

}  // namespace

RemoveWebAppJob::RemoveWebAppJob(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    AppId app_id,
    bool is_initial_request)
    : uninstall_source_(uninstall_source),
      profile_(profile),
      app_id_(app_id),
      is_initial_request_(is_initial_request) {
  if (is_initial_request_) {
    CHECK(CanUninstallAllManagementSources(uninstall_source_));
  }
}

RemoveWebAppJob::~RemoveWebAppJob() = default;

void RemoveWebAppJob::Start(AllAppsLock& lock, Callback callback) {
  lock_ = &lock;
  callback_ = std::move(callback);

  const WebApp* app = lock_->registrar().GetAppById(app_id_);
  if (!app) {
    CompleteAndSelfDestruct(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  if (app->isolation_data().has_value()) {
    has_isolated_storage_ = true;
  }

  if (is_initial_request_) {
    // The following CHECK streamlines the user uninstall and sync uninstall
    // flow, because for sync uninstalls, the web_app source is removed before
    // being synced, so the first condition fails by the time an Uninstall is
    // invoked.
    // TODO(crbug.com/1447308): Checking kSync shouldn't be needed once
    // this issue is resolved.
    // TODO(crbug.com/1427340): Change this to be:
    // if (uninstall_source is user initiated) {
    //   CHECK(user can uninstall);
    //   Add to user uninstalled prefs.
    // }
    CHECK(app->CanUserUninstallWebApp() ||
          uninstall_source_ == webapps::WebappUninstallSource::kSync);

    if (app->IsPreinstalledApp()) {
      // Update the default uninstalled web_app prefs if it is a preinstalled
      // app but being removed by user.
      const WebApp::ExternalConfigMap& config_map =
          app->management_to_external_config_map();
      auto it = config_map.find(WebAppManagement::kDefault);
      if (it != config_map.end()) {
        UserUninstalledPreinstalledWebAppPrefs(profile_->GetPrefs())
            .Add(app_id_, it->second.install_urls);
      } else {
        base::UmaHistogramBoolean(
            "WebApp.Preinstalled.ExternalConfigMapAbsentDuringUninstall", true);
      }
    }
  }

  sub_apps_pending_removal_ = lock_->registrar().GetAllSubAppIds(app_id_);

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

    url::Origin iwa_origin = url::Origin::Create(app->scope());
    web_app::RemoveIsolatedWebAppBrowsingData(
        &profile_.get(), iwa_origin,
        base::BindOnce(&RemoveWebAppJob::OnIsolatedWebAppBrowsingDataCleared,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  auto synchronize_barrier = OsIntegrationManager::GetBarrierForSynchronize(
      base::BindOnce(&RemoveWebAppJob::OnOsHooksUninstalled,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/1401125): Remove UninstallAllOsHooks() once OS integration
  // sub managers have been implemented.
  lock_->os_integration_manager().UninstallAllOsHooks(app_id_,
                                                      synchronize_barrier);
  lock_->os_integration_manager().Synchronize(
      app_id_, base::BindOnce(synchronize_barrier, OsHooksErrors()));

  // While sometimes `Synchronize` needs to read icon data, for the uninstall
  // case it never needs to be read. Thus, it is safe to schedule this now and
  // not after the `Synchronize` call completes.
  lock_->icon_manager().DeleteData(
      app_id_, base::BindOnce(&RemoveWebAppJob::OnIconDataDeleted,
                              weak_ptr_factory_.GetWeakPtr()));

  lock_->translation_manager().DeleteTranslations(
      app_id_, base::BindOnce(&RemoveWebAppJob::OnTranslationDataDeleted,
                              weak_ptr_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (ResolveExperimentalWebAppIsolationFeature() ==
      ExperimentalWebAppIsolationMode::kProfile) {
    if (app->chromeos_data() && app->chromeos_data()->app_profile_path) {
      const base::FilePath& app_profile_path =
          app->chromeos_data()->app_profile_path.value();
      CHECK(Profile::IsWebAppProfilePath(app_profile_path));
      if (g_browser_process->profile_manager()
              ->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(app_profile_path)) {
        pending_app_profile_deletion_ = true;
        g_browser_process->profile_manager()
            ->GetDeleteProfileHelper()
            .MaybeScheduleProfileForDeletion(
                app_profile_path,
                base::BindOnce(&RemoveWebAppJob::OnWebAppProfileDeleted,
                               weak_ptr_factory_.GetWeakPtr()),
                ProfileMetrics::ProfileDelete::DELETE_PROFILE_USER_MANAGER);
      } else {
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

base::Value RemoveWebAppJob::ToDebugValue() const {
  base::Value::Dict dict;
  dict.Set("!job", "RemoveWebAppJob");
  dict.Set("app_id", app_id_);
  dict.Set("is_initial_request", is_initial_request_);
  dict.Set("callback", callback_.is_null());
  dict.Set("app_data_deleted", app_data_deleted_);
  dict.Set("translation_data_deleted", translation_data_deleted_);
  dict.Set("hooks_uninstalled", hooks_uninstalled_);
  dict.Set("pending_app_profile_deletion", pending_app_profile_deletion_);
  dict.Set("errors", errors_);
  dict.Set("primary_removal_result",
           primary_removal_result_
               ? base::Value(base::ToString(primary_removal_result_.value()))
               : base::Value());
  {
    base::Value::List list;
    for (const AppId& sub_app_id : sub_apps_pending_removal_) {
      list.Append(sub_app_id);
    }
    dict.Set("sub_apps_pending_removal", std::move(list));
  }
  dict.Set("active_sub_job",
           sub_job_ ? sub_job_->ToDebugValue() : base::Value());
  dict.Set("completed_sub_jobs", completed_sub_job_debug_dict_.Clone());
  return base::Value(std::move(dict));
}

webapps::WebappUninstallSource RemoveWebAppJob::uninstall_source() const {
  return uninstall_source_;
}

void RemoveWebAppJob::OnOsHooksUninstalled(OsHooksErrors errors) {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!hooks_uninstalled_);
  hooks_uninstalled_ = true;
  base::UmaHistogramBoolean("WebApp.Uninstall.OsHookSuccess", errors.none());
  errors_ = errors_ || errors.any();
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::OnIconDataDeleted(bool success) {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!app_data_deleted_);
  app_data_deleted_ = true;
  base::UmaHistogramBoolean("WebApp.Uninstall.IconDataSuccess", success);
  errors_ = errors_ || !success;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::OnTranslationDataDeleted(bool success) {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!translation_data_deleted_);
  translation_data_deleted_ = true;
  errors_ = errors_ || !success;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::OnWebAppProfileDeleted(Profile* profile) {
  CHECK(!primary_removal_result_.has_value());
  CHECK(pending_app_profile_deletion_);
  // This must be an isolated web app profile rather than the WebAppProvider
  // profile.
  CHECK_NE(&profile_.get(), profile);
  pending_app_profile_deletion_ = false;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::OnIsolatedWebAppBrowsingDataCleared() {
  CHECK(!primary_removal_result_.has_value());
  CHECK(!isolated_web_app_browsing_data_cleared_);
  // Must be an Isolated Web App.
  CHECK(has_isolated_storage_);
  isolated_web_app_browsing_data_cleared_ = true;
  MaybeFinishPrimaryRemoval();
}

void RemoveWebAppJob::MaybeFinishPrimaryRemoval() {
  CHECK(!primary_removal_result_.has_value());
  if (!hooks_uninstalled_ || !app_data_deleted_ || !translation_data_deleted_ ||
      pending_app_profile_deletion_ ||
      (has_isolated_storage_ && !isolated_web_app_browsing_data_cleared_)) {
    return;
  }

  {
    CHECK_NE(lock_->registrar().GetAppById(app_id_), nullptr);
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    update->DeleteApp(app_id_);
  }

  primary_removal_result_ = errors_ ? webapps::UninstallResultCode::kError
                                    : webapps::UninstallResultCode::kSuccess;
  base::UmaHistogramBoolean("WebApp.Uninstall.Result", !errors_);

  lock_->install_manager().NotifyWebAppUninstalled(app_id_, uninstall_source_);

  ProcessSubAppsPendingRemovalOrComplete();
}

void RemoveWebAppJob::ProcessSubAppsPendingRemovalOrComplete() {
  CHECK(primary_removal_result_.has_value());

  if (sub_job_) {
    completed_sub_job_debug_dict_.Set(sub_job_->app_id(),
                                      sub_job_->ToDebugValue());
    sub_job_.reset();
  }

  if (sub_apps_pending_removal_.empty()) {
    CompleteAndSelfDestruct(primary_removal_result_.value());
    return;
  }

  AppId sub_app_id = std::move(sub_apps_pending_removal_.back());
  sub_apps_pending_removal_.pop_back();

  sub_job_ = std::make_unique<RemoveInstallSourceJob>(
      uninstall_source_, profile_.get(), sub_app_id,
      WebAppManagement::Type::kSubApp);
  sub_job_->Start(*lock_,
                  base::IgnoreArgs<webapps::UninstallResultCode>(base::BindOnce(
                      &RemoveWebAppJob::ProcessSubAppsPendingRemovalOrComplete,
                      weak_ptr_factory_.GetWeakPtr())));
}

void RemoveWebAppJob::CompleteAndSelfDestruct(
    webapps::UninstallResultCode code) {
  CHECK(callback_);
  std::move(callback_).Run(code);
}

}  // namespace web_app
