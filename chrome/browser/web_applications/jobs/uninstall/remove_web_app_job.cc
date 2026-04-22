// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/constants.h"

namespace web_app {

namespace {

bool IsOsIntegrationRemovedForApp(
    std::optional<proto::os_state::WebAppOsIntegration> state) {
  if (!state.has_value()) {
    return true;
  }

  return !state->has_file_handling() && !state->has_protocols_handled() &&
         !state->has_run_on_os_login() && !state->has_shortcut() &&
         !state->has_shortcut_menus() && !state->has_uninstall_registration();
}

void DeleteFolderAndReply(scoped_refptr<FileUtilsWrapper> file_utils,
                          const base::FilePath& folder,
                          base::OnceClosure done) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          base::IgnoreResult(&FileUtilsWrapper::DeleteFileRecursively),
          file_utils, folder),
      std::move(done));
}

}  // namespace

RemoveWebAppJob::RemoveWebAppJob(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    base::DictValue& debug_value,
    webapps::AppId app_id)
    : uninstall_source_(uninstall_source),
      profile_(profile),
      debug_value_(debug_value),
      app_id_(app_id) {
  base::DictValue dict;
  debug_value_->Set("!job", "RemoveWebAppJob");
  debug_value_->Set("app_id", app_id_);
}

// static
void RemoveWebAppJob::RemoveForCorruptDatabase(
    WebAppProvider& provider,
    const std::vector<std::pair<webapps::AppId, GURL>>& salvaged_apps,
    base::OnceClosure done) {
  // This code holds a scoped profile keep alive to keep the profile alive,
  // first deletes the database, then all other data.
  // This is done because:
  // 1. The common path, where the profile keep alive is acquired, will make
  //    sure all data is deleted correctly.
  // 2. If there is a crash in this cleanup code, it is high-risk as it runs on
  //    startup. By deleting the database first, we de-risk this scenario as
  //    this means that the rest of this code will not get executed again on
  //    startup, reducing the crash risk to only the database deletion lines of
  //    code.
  // The alternative would be to delete the database data last, so that any
  // profile shutdown happening during execution would mean that the deletions
  // happen again to recover. However, the crash-on-startup risk is high for
  // not-frequently-used-code, so using a ProfileKeepAlive with less risky code
  // seems like a better compromise.

  // Note: This can fail, so the code has to be resilient to the profile being
  // deleted.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive =
      ScopedProfileKeepAlive::TryAcquire(
          provider.profile(),
          ProfileKeepAliveOrigin::kWebAppDatabaseCorruptionRecovery);

  base::OnceCallback<void(base::OnceClosure)> remove_non_database_app_data =
      base::BindOnce(
          [](base::WeakPtr<WebAppProvider> provider,
             std::vector<std::pair<webapps::AppId, GURL>> salvaged_apps,
             base::OnceClosure done) {
            if (!provider) {
              std::move(done).Run();
              return;
            }

            scoped_refptr<FileUtilsWrapper> file_utils = provider->file_utils();

            base::ConcurrentClosures concurrent;
            for (const auto& [app_id, start_url] : salvaged_apps) {
              base::FilePath os_integration_dir =
                  GetOsIntegrationResourcesDirectoryForApp(
                      provider->profile()->GetPath(), app_id, start_url);

              provider->os_integration_manager().Synchronize(
                  app_id,
                  base::BindOnce(&DeleteFolderAndReply, file_utils,
                                 os_integration_dir,
                                 concurrent.CreateClosure()),
                  SynchronizeOsOptions{.force_unregister_os_integration =
                                           true});

              provider->icon_manager().DeleteData(
                  app_id, base::IgnoreArgs<bool>(concurrent.CreateClosure()));
              provider->translation_manager().DeleteTranslations(
                  app_id, base::IgnoreArgs<bool>(concurrent.CreateClosure()));
            }
            std::move(concurrent).Done(std::move(done));
          },
          provider.AsWeakPtr(), salvaged_apps);

  base::OnceClosure final_cleanup = base::BindOnce(
      [](std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive) {
        if (!profile_keep_alive) {
          return;
        }
        PrefService* prefs = profile_keep_alive->profile()->GetPrefs();
        prefs->ClearPref(prefs::kWebAppsPreferences);
        prefs->ClearPref(prefs::kWebAppsDailyMetrics);
        prefs->ClearPref(prefs::kWebAppsAppAgnosticIphState);
        prefs->ClearPref(prefs::kWebAppsAppAgnosticMlState);
        prefs->ClearPref(prefs::kWebAppsAppAgnosticIPHLinkCapturingState);
        prefs->ClearPref(prefs::kWebAppsLastPreinstallSynchronizeVersion);
        prefs->ClearPref(webapps::kWebAppsMigratedPreinstalledApps);
        prefs->ClearPref(prefs::kWebAppsDidMigrateDefaultChromeApps);
        prefs->ClearPref(prefs::kWebAppsUninstalledDefaultChromeApps);
        prefs->ClearPref(prefs::kAppShortcutsVersion);
        prefs->ClearPref(prefs::kAppShortcutsArch);
        prefs->ClearPref(prefs::kIsolatedWebAppPendingInitializationCount);
        prefs->ClearPref(prefs::kIsolatedWebAppUserInstallationEnabled);
        prefs->SetBoolean(prefs::kShouldGarbageCollectStoragePartitions, true);
      },
      std::move(profile_keep_alive));

  base::OnceClosure do_cleanup_then_complete =
      base::BindOnce(std::move(remove_non_database_app_data),
                     std::move(final_cleanup).Then(std::move(done)));
  // Always re-fetch the data store, as the corruption can happen before the
  // data store is fetched.
  provider.database_factory().GetStoreFactory().Run(
      syncer::DataType::WEB_APPS,
      base::BindOnce(
          [](base::OnceClosure cleanup_and_complete,
             const std::optional<syncer::ModelError>& error,
             std::unique_ptr<syncer::DataTypeStore> data_store) {
            if (!data_store || error.has_value()) {
              DLOG(ERROR) << "Could not open up webapp datastore for "
                             "corruption recovery. "
                          << (error.has_value() ? error->ToString() : "");
              data_store.reset();
              std::move(cleanup_and_complete).Run();
              return;
            }
            auto* store_ptr = data_store.get();
            store_ptr->DeleteAllDataAndMetadata(
                /*metadata_change_list=*/nullptr,
                base::BindOnce(
                    [](std::unique_ptr<syncer::DataTypeStore> store,
                       const std::optional<syncer::ModelError>& error) {
                      // This callback is only here to print out the error and
                      // 'own' the store so it stays alive during deletion.
                      // Note: it is important to delete the store before
                      // calling the callback to prevent multiple model stores
                      // for the same type from existing at the same time.
                      if (error.has_value()) {
                        DLOG(ERROR)
                            << "Could not delete webapp data and metadata: "
                            << error->ToString();
                      }
                    },
                    std::move(data_store))
                    .Then(std::move(cleanup_and_complete)));
          },
          std::move(do_cleanup_then_complete)));
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
  base::ListValue* list = debug_value_->EnsureList("sub_apps_found");
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
  bool os_integration_removal_success = IsOsIntegrationRemovedForApp(
      lock_->registrar().GetAppCurrentOsIntegrationState(app_id_));
  debug_value_->Set("os_integration_removal_success",
                    os_integration_removal_success);
  errors_ = errors_ || !os_integration_removal_success;
  GURL start_url = lock_->registrar().GetAppStartUrl(app_id_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          [](Profile& profile, const webapps::AppId& app_id,
             const GURL& start_url) {
            return base::DeletePathRecursively(
                GetOsIntegrationResourcesDirectoryForApp(profile.GetPath(),
                                                         app_id, start_url));
          },
          std::ref(profile_.get()), app_id_, std::move(start_url)),
      base::BindOnce(
          &RemoveWebAppJob::OnOsResourcesCleanedMaybeCompleteUninstall,
          weak_ptr_factory_.GetWeakPtr()));
}

void RemoveWebAppJob::OnOsResourcesCleanedMaybeCompleteUninstall(
    bool os_integration_directory_removed) {
  CHECK(!hooks_uninstalled_);
  hooks_uninstalled_ = true;
  debug_value_->Set("os_integration_directory_removed",
                    os_integration_directory_removed);
  errors_ = errors_ || !os_integration_directory_removed;
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
