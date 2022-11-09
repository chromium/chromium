// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_data_fetch_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/update_file_handler_command.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

WebAppCommandScheduler::WebAppCommandScheduler(WebAppProvider* provider)
    : provider_(provider) {}

WebAppCommandScheduler::~WebAppCommandScheduler() = default;

void WebAppCommandScheduler::FetchManifestAndInstall(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    bool use_fallback) {
  if (is_in_shutdown_)
    return;

  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&WebAppCommandScheduler::FetchManifestAndInstall,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(install_surface), std::move(contents),
                       bypass_service_worker_check, std::move(dialog_callback),
                       std::move(callback), use_fallback));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          std::move(install_surface), std::move(contents),
          bypass_service_worker_check, std::move(dialog_callback),
          std::move(callback), use_fallback,
          std::make_unique<WebAppDataRetriever>()));
}

void WebAppCommandScheduler::PersistFileHandlersUserChoice(
    const AppId& app_id,
    bool allowed,
    base::OnceClosure callback) {
  if (is_in_shutdown_)
    return;

  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&WebAppCommandScheduler::PersistFileHandlersUserChoice,
                       weak_ptr_factory_.GetWeakPtr(), app_id, allowed,
                       std::move(callback)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      UpdateFileHandlerCommand::CreateForPersistUserChoice(
          app_id, allowed, std::move(callback)));
}

void WebAppCommandScheduler::UpdateFileHandlerOsIntegration(
    const AppId& app_id,
    base::OnceClosure callback) {
  if (is_in_shutdown_)
    return;

  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&WebAppCommandScheduler::UpdateFileHandlerOsIntegration,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(callback)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      UpdateFileHandlerCommand::CreateForUpdate(app_id, std::move(callback)));
}

void WebAppCommandScheduler::ScheduleManifestUpdateDataFetch(
    const GURL& url,
    const AppId& app_id,
    base::WeakPtr<content::WebContents> contents,
    ManifestFetchCallback callback) {
  if (is_in_shutdown_)
    return;

  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&WebAppCommandScheduler::ScheduleManifestUpdateDataFetch,
                       weak_ptr_factory_.GetWeakPtr(), url, app_id, contents,
                       std::move(callback)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateDataFetchCommand>(
          url, app_id, contents, std::move(callback), &provider_->registrar(),
          &provider_->icon_manager(), &provider_->ui_manager(),
          &provider_->os_integration_manager(),
          std::make_unique<WebAppDataRetriever>()));
}

void WebAppCommandScheduler::ScheduleManifestUpdateFinalize(
    const GURL& url,
    const AppId& app_id,
    WebAppInstallInfo install_info,
    bool app_identity_update_allowed,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    ManifestWriteCallback callback) {
  if (is_in_shutdown_)
    return;

  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&WebAppCommandScheduler::ScheduleManifestUpdateFinalize,
                       weak_ptr_factory_.GetWeakPtr(), url, app_id,
                       std::move(install_info), app_identity_update_allowed,
                       std::move(keep_alive), std::move(profile_keep_alive),
                       std::move(callback)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateFinalizeCommand>(
          url, app_id, std::move(install_info), app_identity_update_allowed,
          std::move(callback), std::move(keep_alive),
          std::move(profile_keep_alive), &provider_->registrar(),
          &provider_->install_finalizer(), &provider_->os_integration_manager(),
          &provider_->sync_bridge()));
}

void WebAppCommandScheduler::FetchInstallabilityForChromeManagement(
    const GURL& url,
    base::WeakPtr<content::WebContents> web_contents,
    FetchInstallabilityForChromeManagementCallback callback) {
  if (is_in_shutdown_)
    return;

  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(
            &WebAppCommandScheduler::FetchInstallabilityForChromeManagement,
            weak_ptr_factory_.GetWeakPtr(), url, web_contents,
            std::move(callback)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<web_app::FetchInstallabilityForChromeManagement>(
          url, web_contents, std::make_unique<web_app::WebAppUrlLoader>(),
          std::make_unique<web_app::WebAppDataRetriever>(),
          std::move(callback)));
}

void WebAppCommandScheduler::Shutdown() {
  is_in_shutdown_ = true;
}

}  // namespace web_app
