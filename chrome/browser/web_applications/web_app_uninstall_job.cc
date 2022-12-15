// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_uninstall_job.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
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

namespace web_app {

// static
std::unique_ptr<WebAppUninstallJob> WebAppUninstallJob::CreateAndStart(
    const AppId& app_id,
    const url::Origin& app_origin,
    UninstallCallback callback,
    OsIntegrationManager& os_integration_manager,
    WebAppSyncBridge& sync_bridge,
    WebAppIconManager& icon_manager,
    WebAppRegistrar& registrar,
    WebAppInstallManager& install_manager,
    WebAppTranslationManager& translation_manager,
    PrefService& profile_prefs) {
  return base::WrapUnique(new WebAppUninstallJob(
      app_id, app_origin, std::move(callback), os_integration_manager,
      sync_bridge, icon_manager, registrar, install_manager,
      translation_manager, profile_prefs));
}

WebAppUninstallJob::WebAppUninstallJob(
    const AppId& app_id,
    const url::Origin& app_origin,
    UninstallCallback callback,
    OsIntegrationManager& os_integration_manager,
    WebAppSyncBridge& sync_bridge,
    WebAppIconManager& icon_manager,
    WebAppRegistrar& registrar,
    WebAppInstallManager& install_manager,
    WebAppTranslationManager& translation_manager,
    PrefService& profile_prefs)
    : app_id_(app_id),
      callback_(std::move(callback)),
      registrar_(&registrar),
      sync_bridge_(&sync_bridge),
      install_manager_(&install_manager) {
  Start(app_origin, os_integration_manager, icon_manager, translation_manager,
        profile_prefs);
}

WebAppUninstallJob::~WebAppUninstallJob() = default;

void WebAppUninstallJob::Start(const url::Origin& app_origin,
                               OsIntegrationManager& os_integration_manager,
                               WebAppIconManager& icon_manager,
                               WebAppTranslationManager& translation_manager,
                               PrefService& profile_prefs) {
  DCHECK(install_manager_);

  DCHECK(state_ == State::kNotStarted);
  state_ = State::kPendingDataDeletion;

  // Note: It is supported to re-start an uninstall on startup, so
  // `is_uninstalling()` is not checked. It is a class invariant that there can
  // never be more than one uninstall task operating on the same web app at the
  // same time.
  {
    ScopedRegistryUpdate update(sync_bridge_);
    WebApp* app = update->UpdateApp(app_id_);
    DCHECK(app);
    app->SetIsUninstalling(true);
  }
  install_manager_->NotifyWebAppWillBeUninstalled(app_id_);

  RemoveAppIsolationState(&profile_prefs, app_origin);

  auto synchronize_barrier = OsIntegrationManager::GetBarrierForSynchronize(
      base::BindOnce(&WebAppUninstallJob::OnOsHooksUninstalled,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/1401125): Remove UninstallAllOsHooks() once OS integration
  // sub managers have been implemented.
  os_integration_manager.UninstallAllOsHooks(app_id_, synchronize_barrier);
  os_integration_manager.Synchronize(
      app_id_, base::BindOnce(synchronize_barrier, OsHooksErrors()));

  icon_manager.DeleteData(app_id_,
                          base::BindOnce(&WebAppUninstallJob::OnIconDataDeleted,
                                         weak_ptr_factory_.GetWeakPtr()));

  translation_manager.DeleteTranslations(
      app_id_, base::BindOnce(&WebAppUninstallJob::OnTranslationDataDeleted,
                              weak_ptr_factory_.GetWeakPtr()));
}

void WebAppUninstallJob::OnOsHooksUninstalled(OsHooksErrors errors) {
  DCHECK(state_ == State::kPendingDataDeletion);
  hooks_uninstalled_ = true;
  base::UmaHistogramBoolean("WebApp.Uninstall.OsHookSuccess", errors.none());
  errors_ = errors_ || errors.any();
  MaybeFinishUninstall();
}

void WebAppUninstallJob::OnIconDataDeleted(bool success) {
  DCHECK(state_ == State::kPendingDataDeletion);
  app_data_deleted_ = true;
  base::UmaHistogramBoolean("WebApp.Uninstall.IconDataSuccess", success);
  errors_ = errors_ || !success;
  MaybeFinishUninstall();
}

void WebAppUninstallJob::OnTranslationDataDeleted(bool success) {
  DCHECK(state_ == State::kPendingDataDeletion);
  translation_data_deleted_ = true;
  errors_ = errors_ || !success;
  MaybeFinishUninstall();
}

void WebAppUninstallJob::MaybeFinishUninstall() {
  DCHECK(state_ == State::kPendingDataDeletion);
  if (!hooks_uninstalled_ || !app_data_deleted_ || !translation_data_deleted_) {
    return;
  }
  state_ = State::kDone;

  base::UmaHistogramBoolean("WebApp.Uninstall.Result", !errors_);
  {
    DCHECK_NE(registrar_->GetAppById(app_id_), nullptr);
    ScopedRegistryUpdate update(sync_bridge_);
    update->DeleteApp(app_id_);
  }
  install_manager_->NotifyWebAppUninstalled(app_id_);
  std::move(callback_).Run(errors_ ? webapps::UninstallResultCode::kError
                                   : webapps::UninstallResultCode::kSuccess);
}

}  // namespace web_app
