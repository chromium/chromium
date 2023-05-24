// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/remove_web_app_job.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
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

RemoveWebAppJob::~RemoveWebAppJob() = default;

std::unique_ptr<RemoveWebAppJob> RemoveWebAppJob::Start(
    webapps::WebappUninstallSource uninstall_source,
    AppId app_id,
    WithAppResources& resources,
    Profile& profile,
    UninstallCallback callback) {
  CHECK(resources.registrar().GetAppById(app_id));

  auto job = base::WrapUnique(new RemoveWebAppJob(
      uninstall_source, app_id, resources, profile, std::move(callback)));
  base::WeakPtr<RemoveWebAppJob> job_weak_ptr =
      job->weak_ptr_factory_.GetWeakPtr();

  resources.install_manager().NotifyWebAppWillBeUninstalled(app_id);

  {
    // Note: It is supported to re-start an uninstall on startup, so
    // `is_uninstalling()` is not checked. It is a class invariant that there
    // can never be more than one uninstall task operating on the same web app
    // at the same time.
    ScopedRegistryUpdate update(&resources.sync_bridge());
    WebApp* app = update->UpdateApp(app_id);
    CHECK(app);
    app->SetIsUninstalling(true);
  }

  auto synchronize_barrier = OsIntegrationManager::GetBarrierForSynchronize(
      base::BindOnce(&RemoveWebAppJob::OnOsHooksUninstalled, job_weak_ptr));

  // TODO(crbug.com/1401125): Remove UninstallAllOsHooks() once OS integration
  // sub managers have been implemented.
  resources.os_integration_manager().UninstallAllOsHooks(app_id,
                                                         synchronize_barrier);
  resources.os_integration_manager().Synchronize(
      app_id, base::BindOnce(synchronize_barrier, OsHooksErrors()));

  // While sometimes `Synchronize` needs to read icon data, for the uninstall
  // case it never needs to be read. Thus, it is safe to schedule this now and
  // not after the `Synchronize` call completes.
  resources.icon_manager().DeleteData(
      app_id,
      base::BindOnce(&RemoveWebAppJob::OnIconDataDeleted, job_weak_ptr));

  resources.translation_manager().DeleteTranslations(
      app_id,
      base::BindOnce(&RemoveWebAppJob::OnTranslationDataDeleted, job_weak_ptr));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (ResolveExperimentalWebAppIsolationFeature() ==
      ExperimentalWebAppIsolationMode::kProfile) {
    const WebApp& app = *resources.registrar().GetAppById(app_id);
    if (app.chromeos_data() && app.chromeos_data()->app_profile_path) {
      const base::FilePath& app_profile_path =
          app.chromeos_data()->app_profile_path.value();
      CHECK(Profile::IsWebAppProfilePath(app_profile_path));
      if (g_browser_process->profile_manager()
              ->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(app_profile_path)) {
        job->pending_app_profile_deletion_ = true;
        g_browser_process->profile_manager()
            ->GetDeleteProfileHelper()
            .MaybeScheduleProfileForDeletion(
                app_profile_path,
                base::BindOnce(&RemoveWebAppJob::OnWebAppProfileDeleted,
                               job_weak_ptr),
                ProfileMetrics::ProfileDelete::DELETE_PROFILE_USER_MANAGER);
      } else {
        LOG(ERROR) << "cannot find web app profile at " << app_profile_path;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return job;
}

RemoveWebAppJob::RemoveWebAppJob(
    webapps::WebappUninstallSource uninstall_source,
    AppId app_id,
    WithAppResources& resources,
    Profile& profile,
    UninstallCallback callback)
    : uninstall_source_(uninstall_source),
      app_id_(app_id),
      resources_(resources),
      profile_(profile),
      callback_(std::move(callback)) {}

void RemoveWebAppJob::OnOsHooksUninstalled(OsHooksErrors errors) {
  CHECK(!done_);
  CHECK(!hooks_uninstalled_);
  hooks_uninstalled_ = true;
  base::UmaHistogramBoolean("WebApp.Uninstall.OsHookSuccess", errors.none());
  errors_ = errors_ || errors.any();
  MaybeFinishUninstall();
}

void RemoveWebAppJob::OnIconDataDeleted(bool success) {
  CHECK(!done_);
  CHECK(!app_data_deleted_);
  app_data_deleted_ = true;
  base::UmaHistogramBoolean("WebApp.Uninstall.IconDataSuccess", success);
  errors_ = errors_ || !success;
  MaybeFinishUninstall();
}

void RemoveWebAppJob::OnTranslationDataDeleted(bool success) {
  CHECK(!done_);
  CHECK(!translation_data_deleted_);
  translation_data_deleted_ = true;
  errors_ = errors_ || !success;
  MaybeFinishUninstall();
}

void RemoveWebAppJob::OnWebAppProfileDeleted(Profile* profile) {
  CHECK(!done_);
  CHECK(pending_app_profile_deletion_);
  // This must be an isolated web app profile rather than the WebAppProvider
  // profile.
  CHECK_NE(&profile_.get(), profile);
  pending_app_profile_deletion_ = false;
  MaybeFinishUninstall();
}

void RemoveWebAppJob::MaybeFinishUninstall() {
  CHECK(!done_);
  if (!hooks_uninstalled_ || !app_data_deleted_ || !translation_data_deleted_ ||
      pending_app_profile_deletion_) {
    return;
  }
  done_ = true;

  base::UmaHistogramBoolean("WebApp.Uninstall.Result", !errors_);
  {
    CHECK_NE(resources_->registrar().GetAppById(app_id_), nullptr);
    ScopedRegistryUpdate update(&resources_->sync_bridge());
    update->DeleteApp(app_id_);
  }
  resources_->install_manager().NotifyWebAppUninstalled(app_id_,
                                                        uninstall_source_);
  std::move(callback_).Run(!errors_);
}

}  // namespace web_app
