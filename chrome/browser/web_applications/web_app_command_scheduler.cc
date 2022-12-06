// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"
#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_data_fetch_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/commands/update_file_handler_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"

namespace web_app {
namespace {

std::unique_ptr<content::WebContents> CreateIsolatedWebAppWebContents(
    Profile& profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          /*context=*/&profile));

  webapps::InstallableManager::CreateForWebContents(web_contents.get());

  return web_contents;
}

}  // namespace

WebAppCommandScheduler::WebAppCommandScheduler(Profile& profile,
                                               WebAppProvider* provider)
    : profile_(profile), provider_(provider) {}

WebAppCommandScheduler::~WebAppCommandScheduler() = default;

void WebAppCommandScheduler::Shutdown() {
  is_in_shutdown_ = true;
}

void WebAppCommandScheduler::FetchManifestAndInstall(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    bool use_fallback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          std::move(install_surface), std::move(contents),
          bypass_service_worker_check, std::move(dialog_callback),
          std::move(callback), use_fallback,
          std::make_unique<WebAppDataRetriever>()));
}

void WebAppCommandScheduler::InstallFromInfo(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(install_callback), AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          std::move(install_info), overwrite_existing_manifest_fields,
          std::move(install_surface), std::move(install_callback)));
}

void WebAppCommandScheduler::InstallFromInfoWithParams(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const WebAppInstallParams& install_params) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(install_callback), AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          std::move(install_info), overwrite_existing_manifest_fields,
          std::move(install_surface), std::move(install_callback),
          install_params));
}

void WebAppCommandScheduler::InstallExternallyManagedApp(
    const ExternalInstallOptions& external_install_options,
    OnceInstallCallback callback,
    base::WeakPtr<content::WebContents> contents,
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), AppId(),
                                  webapps::InstallResultCode::
                                      kCancelledOnWebAppProviderShuttingDown));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ExternallyManagedInstallCommand>(
          external_install_options, std::move(callback), contents,
          std::move(data_retriever)));
}

void WebAppCommandScheduler::PersistFileHandlersUserChoice(
    const AppId& app_id,
    bool allowed,
    base::OnceClosure callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      UpdateFileHandlerCommand::CreateForPersistUserChoice(
          app_id, allowed, std::move(callback)));
}

void WebAppCommandScheduler::UpdateFileHandlerOsIntegration(
    const AppId& app_id,
    base::OnceClosure callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback)));
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
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ManifestUpdateResult::kWebContentsDestroyed,
                                  /*install_info_=*/absl::nullopt,
                                  /*app_identity_update_allowed=*/false));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateDataFetchCommand>(
          url, app_id, contents, std::move(callback),
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
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  /*url=*/GURL(),
                                  /*app_id=*/AppId(),
                                  ManifestUpdateResult::kWebContentsDestroyed));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateFinalizeCommand>(
          url, app_id, std::move(install_info), app_identity_update_allowed,
          std::move(callback), std::move(keep_alive),
          std::move(profile_keep_alive)));
}

void WebAppCommandScheduler::FetchInstallabilityForChromeManagement(
    const GURL& url,
    base::WeakPtr<content::WebContents> web_contents,
    FetchInstallabilityForChromeManagementCallback callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  InstallableCheckResult::kNotInstallable,
                                  /*app_id=*/absl::nullopt));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<web_app::FetchInstallabilityForChromeManagement>(
          url, web_contents, std::make_unique<web_app::WebAppUrlLoader>(),
          std::make_unique<web_app::WebAppDataRetriever>(),
          std::move(callback)));
}

void WebAppCommandScheduler::InstallIsolatedWebApp(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolationData& isolation_data,
    InstallIsolatedWebAppCallback callback) {
  if (IsShuttingDown()) {
    InstallIsolatedWebAppCommandError error;
    error.message = "The profile and/or browser are shutting down.";
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error)));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallIsolatedWebAppCommand>(
          url_info, isolation_data, CreateIsolatedWebAppWebContents(*profile_),
          std::make_unique<WebAppUrlLoader>(), *profile_, std::move(callback)));
}

void WebAppCommandScheduler::SetRunOnOsLoginMode(const AppId& app_id,
                                                 RunOnOsLoginMode login_mode,
                                                 base::OnceClosure callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(app_id, std::move(login_mode),
                                                 std::move(callback)));
}

void WebAppCommandScheduler::SyncRunOnOsLoginMode(const AppId& app_id,
                                                  base::OnceClosure callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSyncLoginMode(app_id, std::move(callback)));
}

void WebAppCommandScheduler::ClearWebAppBrowsingData(
    const base::Time& begin_time,
    const base::Time& end_time,
    base::OnceClosure done) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(done));
    return;
  }

  provider_->scheduler().ScheduleCallbackWithLock<FullSystemLock>(
      "ClearWebAppBrowsingData", std::make_unique<FullSystemLockDescription>(),
      base::BindOnce(web_app::ClearWebAppBrowsingData, begin_time, end_time,
                     std::move(done)));
}

void WebAppCommandScheduler::SetAppIsDisabled(const AppId& app_id,
                                              bool is_disabled,
                                              base::OnceClosure callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
    return;
  }

  provider_->scheduler().ScheduleCallbackWithLock<web_app::AppLock>(
      "SetAppIsDisabled",
      std::make_unique<web_app::AppLockDescription,
                       base::flat_set<web_app::AppId>>({app_id}),
      base::BindOnce(
          [](const web_app::AppId& app_id, bool is_disabled,
             web_app::AppLock& lock) {
            lock.sync_bridge().SetAppIsDisabled(lock, app_id, is_disabled);
          },
          app_id, is_disabled));
}

template <class LockType, class DescriptionType>
void WebAppCommandScheduler::ScheduleCallbackWithLock(
    const std::string& operation_name,
    std::unique_ptr<DescriptionType> lock_description,
    base::OnceCallback<void(LockType& lock)> callback) {
  if (IsShuttingDown())
    return;

  provider_->command_manager().ScheduleCommand(
      std::make_unique<CallbackCommand<LockType>>(
          operation_name, std::move(lock_description), std::move(callback)));
}

template <class LockType, class DescriptionType>
void WebAppCommandScheduler::ScheduleCallbackWithLock(
    const std::string& operation_name,
    std::unique_ptr<DescriptionType> lock_description,
    base::OnceCallback<base::Value(LockType& lock)> callback) {
  if (IsShuttingDown())
    return;

  provider_->command_manager().ScheduleCommand(
      std::make_unique<CallbackCommand<LockType>>(
          operation_name, std::move(lock_description), std::move(callback)));
}

bool WebAppCommandScheduler::IsShuttingDown() const {
  return is_in_shutdown_ ||
         KeepAliveRegistry::GetInstance()->IsShuttingDown() ||
         profile_->ShutdownStarted();
}

template void WebAppCommandScheduler::ScheduleCallbackWithLock<NoopLock>(
    const std::string& operation_name,
    std::unique_ptr<NoopLock::LockDescription> lock_description,
    base::OnceCallback<void(NoopLock& lock)> callback);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<NoopLock>(
    const std::string& operation_name,
    std::unique_ptr<NoopLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(NoopLock& lock)> callback);

template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsLock::LockDescription> lock_description,
    base::OnceCallback<void(SharedWebContentsLock& lock)> callback);
template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(SharedWebContentsLock& lock)> callback);

template void WebAppCommandScheduler::ScheduleCallbackWithLock<AppLock>(
    const std::string& operation_name,
    std::unique_ptr<AppLock::LockDescription> lock_description,
    base::OnceCallback<void(AppLock& lock)> callback);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<AppLock>(
    const std::string& operation_name,
    std::unique_ptr<AppLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(AppLock& lock)> callback);

template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsWithAppLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsWithAppLock::LockDescription>
        lock_description,
    base::OnceCallback<void(SharedWebContentsWithAppLock& lock)> callback);
template void
WebAppCommandScheduler::ScheduleCallbackWithLock<SharedWebContentsWithAppLock>(
    const std::string& operation_name,
    std::unique_ptr<SharedWebContentsWithAppLock::LockDescription>
        lock_description,
    base::OnceCallback<base::Value(SharedWebContentsWithAppLock& lock)>
        callback);

template void WebAppCommandScheduler::ScheduleCallbackWithLock<FullSystemLock>(
    const std::string& operation_name,
    std::unique_ptr<FullSystemLock::LockDescription> lock_description,
    base::OnceCallback<void(FullSystemLock& lock)> callback);
template void WebAppCommandScheduler::ScheduleCallbackWithLock<FullSystemLock>(
    const std::string& operation_name,
    std::unique_ptr<FullSystemLock::LockDescription> lock_description,
    base::OnceCallback<base::Value(FullSystemLock& lock)> callback);

}  // namespace web_app
