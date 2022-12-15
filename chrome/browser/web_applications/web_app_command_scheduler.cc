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
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"
#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/commands/install_from_sync_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_data_fetch_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/commands/update_file_handler_command.h"
#include "chrome/browser/web_applications/commands/update_protocol_handler_approval_command.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
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
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
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
    : profile_(profile),
      provider_(provider),
      url_loader_(std::make_unique<WebAppUrlLoader>()) {}

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

void WebAppCommandScheduler::InstallFromSync(const WebApp& web_app,
                                             OnceInstallCallback callback) {
  DCHECK(web_app.is_from_sync_and_pending_installation());
  InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
      web_app.app_id(), web_app.manifest_id(), web_app.start_url(),
      web_app.sync_fallback_data().name, web_app.sync_fallback_data().scope,
      web_app.sync_fallback_data().theme_color, web_app.user_display_mode(),
      web_app.sync_fallback_data().icon_infos);
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromSyncCommand>(
          url_loader_.get(), &profile_.get(),
          std::make_unique<WebAppDataRetriever>(), params,
          std::move(callback)));
}

void WebAppCommandScheduler::Uninstall(
    const AppId& app_id,
    absl::optional<WebAppManagement::Type> external_install_source,
    webapps::WebappUninstallSource uninstall_source,
    WebAppUninstallCommand::UninstallWebAppCallback callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  webapps::UninstallResultCode::kCancelled));
    return;
  }
  provider_->command_manager().ScheduleCommand(
      std::make_unique<WebAppUninstallCommand>(
          app_id, external_install_source, uninstall_source,
          std::move(callback), &profile_.get()));
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

void WebAppCommandScheduler::UpdateProtocolHandlerUserApproval(
    const AppId& app_id,
    const std::string& protocol_scheme,
    bool allowed,
    base::OnceClosure callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
    return;
  }

  provider_->command_manager().ScheduleCommand(
      std::make_unique<UpdateProtocolHandlerApprovalCommand>(
          app_id, protocol_scheme, allowed, std::move(callback)));
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

void WebAppCommandScheduler::LaunchApp(
    const AppId& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const absl::optional<GURL>& url_handler_launch_url,
    const absl::optional<GURL>& protocol_handler_launch_url,
    const absl::optional<GURL>& file_launch_url,
    const std::vector<base::FilePath>& launch_files,
    LaunchWebAppCallback callback) {
  LaunchApp(WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
                app_id, command_line, current_directory, url_handler_launch_url,
                protocol_handler_launch_url, file_launch_url, launch_files),
            LaunchWebAppWindowSetting::kOverrideWithWebAppConfig,
            std::move(callback));
}

void WebAppCommandScheduler::LaunchAppWithCustomParams(
    apps::AppLaunchParams params,
    LaunchWebAppCallback callback) {
  LaunchApp(std::move(params), LaunchWebAppWindowSetting::kUseLaunchParams,
            std::move(callback));
}

void WebAppCommandScheduler::LaunchApp(apps::AppLaunchParams params,
                                       LaunchWebAppWindowSetting option,
                                       LaunchWebAppCallback callback) {
  if (IsShuttingDown()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr, nullptr,
                                  apps::LaunchContainer::kLaunchContainerNone));
    return;
  }
  // Off the record profiles cannot be 'kept alive'.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive =
      profile_->IsOffTheRecord()
          ? nullptr
          : std::make_unique<ScopedProfileKeepAlive>(
                &profile_.get(), ProfileKeepAliveOrigin::kAppWindow);
  std::unique_ptr<ScopedKeepAlive> browser_keep_alive =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::WEB_APP_LAUNCH,
                                        KeepAliveRestartOption::ENABLED);

  auto launch_with_keep_alives = base::BindOnce(
      &WebAppCommandScheduler::LaunchAppWithKeepAlives,
      weak_ptr_factory_.GetWeakPtr(), std::move(params), std::move(option),
      std::move(callback), std::move(profile_keep_alive),
      std::move(browser_keep_alive));
  // Because we are accessing the WebAppUiManager, we should wait until the
  // provider has started to actually create the command.
  if (!provider_->is_registry_ready()) {
    provider_->on_registry_ready().Post(FROM_HERE,
                                        std::move(launch_with_keep_alives));
    return;
  }
  std::move(launch_with_keep_alives).Run();
}

void WebAppCommandScheduler::LaunchAppWithKeepAlives(
    apps::AppLaunchParams params,
    LaunchWebAppWindowSetting launch_setting,
    LaunchWebAppCallback callback,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    std::unique_ptr<ScopedKeepAlive> browser_keep_alive) {
  DCHECK(provider_->is_registry_ready());

  // Decorate the callback to ensure the keep alives are kept alive during the
  // execution of the launch.
  callback = std::move(callback).Then(base::BindOnce(
      [](std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
         std::unique_ptr<ScopedKeepAlive> browser_keep_alive) {},
      std::move(profile_keep_alive), std::move(browser_keep_alive)));

  // Unretained is safe because this callback is lives on the WebAppProvider
  // (via the WebAppCommandManager), which is a Profile KeyedService. It is
  // destructed when the profile is shutting down as well. So it is impossible
  // for this callback to be run with the WebAppUiManager being destructed.
  AppId app_id = params.app_id;
  ScheduleCallbackWithLock(
      "LaunchApp",
      std::make_unique<AppLockDescription, base::flat_set<AppId>>({app_id}),
      base::BindOnce(&WebAppUiManager::LaunchWebApp,
                     base::Unretained(&provider_->ui_manager()),
                     std::move(params), launch_setting, std::ref(*profile_),
                     std::move(callback)));
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
