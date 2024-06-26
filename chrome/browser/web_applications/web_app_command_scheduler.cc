// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include <memory>
#include <optional>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"
#include "chrome/browser/web_applications/commands/compute_app_size_command.h"
#include "chrome/browser/web_applications/commands/dedupe_install_urls_command.h"
#include "chrome/browser/web_applications/commands/external_app_resolution_command.h"
#include "chrome/browser/web_applications/commands/fetch_install_info_from_install_url_command.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/commands/install_app_locally_command.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/commands/install_from_sync_command.h"
#include "chrome/browser/web_applications/commands/internal/callback_command.h"
#include "chrome/browser/web_applications/commands/launch_web_app_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_check_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/navigate_and_trigger_install_dialog_command.h"
#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/commands/set_user_display_mode_command.h"
#include "chrome/browser/web_applications/commands/update_file_handler_command.h"
#include "chrome/browser/web_applications/commands/update_protocol_handler_approval_command.h"
#include "chrome/browser/web_applications/commands/web_app_icon_diagnostic_command.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/cleanup_orphaned_isolated_web_apps_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/get_controlled_frame_partition_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/jobs/link_capturing.h"
#endif

namespace web_app {

WebAppCommandScheduler::WebAppCommandScheduler(Profile& profile)
    : profile_(profile) {}

WebAppCommandScheduler::~WebAppCommandScheduler() = default;

void WebAppCommandScheduler::SetProvider(base::PassKey<WebAppProvider>,
                                         WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppCommandScheduler::Shutdown() {
  is_in_shutdown_ = true;
}

void WebAppCommandScheduler::FetchManifestAndInstall(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    FallbackBehavior behavior,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchManifestAndInstallCommand>(
          install_surface, std::move(contents), std::move(dialog_callback),
          std::move(callback), behavior, provider_->ui_manager().GetWeakPtr()),
      location);
}

void WebAppCommandScheduler::FetchInstallInfoFromInstallUrl(
    webapps::ManifestId manifest_id,
    GURL install_url,
    webapps::ManifestId parent_manifest_id,
    base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchInstallInfoFromInstallUrlCommand>(
          std::move(manifest_id), std::move(install_url),
          std::move(parent_manifest_id), std::move(callback)));
}

void WebAppCommandScheduler::FetchInstallInfoFromInstallUrl(
    webapps::ManifestId manifest_id,
    GURL install_url,
    base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<FetchInstallInfoFromInstallUrlCommand>(
          std::move(manifest_id), std::move(install_url), std::nullopt,
          std::move(callback)));
}

void WebAppCommandScheduler::InstallFromInfoNoIntegrationForTesting(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const base::Location& location) {
  CHECK_IS_TEST();
  WebAppInstallParams params;
  params.install_state = proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
  params.add_to_applications_menu = false;
  params.add_to_desktop = false;
  params.add_to_quick_launch_bar = false;
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          &profile_.get(), std::move(install_info),
          overwrite_existing_manifest_fields, install_surface,
          std::move(install_callback), params),
      location);
}

void WebAppCommandScheduler::InstallFromInfoWithParams(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const WebAppInstallParams& install_params,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromInfoCommand>(
          &profile_.get(), std::move(install_info),
          overwrite_existing_manifest_fields, install_surface,
          std::move(install_callback), install_params),
      location);
}

void WebAppCommandScheduler::InstallExternallyManagedApp(
    const ExternalInstallOptions& external_install_options,
    std::optional<webapps::AppId> installed_placeholder_app_id,
    ExternalAppResolutionCommand::InstalledCallback installed_callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<ExternalAppResolutionCommand>(
          *profile_, external_install_options,
          std::move(installed_placeholder_app_id),
          std::move(installed_callback)),
      location);
}

void WebAppCommandScheduler::PersistFileHandlersUserChoice(
    const webapps::AppId& app_id,
    bool allowed,
    base::OnceClosure callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      UpdateFileHandlerCommand::CreateForPersistUserChoice(app_id, allowed,
                                                           std::move(callback)),
      location);
}

void WebAppCommandScheduler::ScheduleManifestUpdateCheck(
    const GURL& url,
    const webapps::AppId& app_id,
    base::Time check_time,
    base::WeakPtr<content::WebContents> contents,
    ManifestUpdateCheckCommand::CompletedCallback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateCheckCommand>(
          url, app_id, check_time, contents, std::move(callback),
          provider_->web_contents_manager().CreateDataRetriever(),
          provider_->web_contents_manager().CreateIconDownloader()),
      location);
}

void WebAppCommandScheduler::ScheduleManifestUpdateFinalize(
    const GURL& url,
    const webapps::AppId& app_id,
    std::unique_ptr<WebAppInstallInfo> install_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    ManifestWriteCallback callback,
    const base::Location& location) {
  CHECK(install_info);
  provider_->command_manager().ScheduleCommand(
      std::make_unique<ManifestUpdateFinalizeCommand>(
          url, app_id, std::move(install_info), std::move(callback),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive)),
      location);
}

void WebAppCommandScheduler::FetchInstallabilityForChromeManagement(
    const GURL& url,
    base::WeakPtr<content::WebContents> web_contents,
    FetchInstallabilityForChromeManagementCallback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<web_app::FetchInstallabilityForChromeManagement>(
          url, web_contents,
          provider_->web_contents_manager().CreateUrlLoader(),
          provider_->web_contents_manager().CreateDataRetriever(),
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::ScheduleNavigateAndTriggerInstallDialog(
    const GURL& install_url,
    const GURL& origin,
    bool is_renderer_initiated,
    NavigateAndTriggerInstallDialogCommandCallback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<NavigateAndTriggerInstallDialogCommand>(
          install_url, origin, is_renderer_initiated, std::move(callback),
          provider_->ui_manager().GetWeakPtr(),
          std::make_unique<webapps::WebAppUrlLoader>(),
          std::make_unique<WebAppDataRetriever>(), &*profile_),
      location);
}

void WebAppCommandScheduler::InstallIsolatedWebApp(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppInstallSource& install_source,
    const std::optional<base::Version>& expected_version,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    InstallIsolatedWebAppCallback callback,
    const base::Location& call_location) {
  CHECK(optional_profile_keep_alive == nullptr ||
        optional_profile_keep_alive->profile() == &*profile_);
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallIsolatedWebAppCommand>(
          url_info, install_source, expected_version,
          IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
              *profile_),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback),
          std::make_unique<IsolatedWebAppInstallCommandHelper>(
              url_info, provider_->web_contents_manager().CreateDataRetriever(),
              IsolatedWebAppInstallCommandHelper::
                  CreateDefaultResponseReaderFactory(*profile_))),
      call_location);
}

void WebAppCommandScheduler::CleanupOrphanedIsolatedApps(
    CleanupOrphanedIsolatedWebAppsCallback callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<CleanupOrphanedIsolatedWebAppsCommand>(
          *profile_, std::move(callback)),
      call_location);
}

void WebAppCommandScheduler::PrepareAndStoreIsolatedWebAppUpdate(
    const IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo& update_info,
    const IsolatedWebAppUrlInfo& url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(IsolatedWebAppUpdatePrepareAndStoreCommandResult)>
        callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<IsolatedWebAppUpdatePrepareAndStoreCommand>(
          update_info, url_info,
          IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
              *profile_),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback),
          std::make_unique<IsolatedWebAppInstallCommandHelper>(
              url_info, provider_->web_contents_manager().CreateDataRetriever(),
              IsolatedWebAppInstallCommandHelper::
                  CreateDefaultResponseReaderFactory(*profile_))),
      call_location);
}

void WebAppCommandScheduler::ApplyPendingIsolatedWebAppUpdate(
    const IsolatedWebAppUrlInfo& url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(
        base::expected<void, IsolatedWebAppApplyUpdateCommandError>)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<IsolatedWebAppApplyUpdateCommand>(
          url_info,
          IsolatedWebAppInstallCommandHelper::CreateIsolatedWebAppWebContents(
              *profile_),
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback),
          std::make_unique<IsolatedWebAppInstallCommandHelper>(
              url_info, provider_->web_contents_manager().CreateDataRetriever(),
              IsolatedWebAppInstallCommandHelper::
                  CreateDefaultResponseReaderFactory(*profile_))),
      call_location);
}

// Given the |bundle_metadata| of a Signed Web Bundle, schedules a command to
// check the installability of the bundle.
void WebAppCommandScheduler::CheckIsolatedWebAppBundleInstallability(
    const SignedWebBundleMetadata& bundle_metadata,
    base::OnceCallback<void(IsolatedInstallabilityCheckResult,
                            std::optional<base::Version>)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<CheckIsolatedWebAppBundleInstallabilityCommand>(
          &profile_.get(), bundle_metadata, std::move(callback)),
      call_location);
}

void WebAppCommandScheduler::GetIsolatedWebAppBrowsingData(
    base::OnceCallback<void(base::flat_map<url::Origin, int64_t>)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<GetIsolatedWebAppBrowsingDataCommand>(
          &profile_.get(), std::move(callback)),
      call_location);
}

void WebAppCommandScheduler::GetControlledFramePartition(
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
        callback,
    const base::Location& location) {
  provider_->scheduler().ScheduleCallbackWithResult(
      "GetControlledFramePartition", AppLockDescription(url_info.app_id()),
      base::BindOnce(&GetControlledFramePartitionWithLock, &profile_.get(),
                     url_info, partition_name, in_memory),
      std::move(callback), /*arg_for_shutdown=*/
      std::optional<content::StoragePartitionConfig>(std::nullopt), location);
}

void WebAppCommandScheduler::InstallFromSync(const WebApp& web_app,
                                             OnceInstallCallback callback,
                                             const base::Location& location) {
  DCHECK(web_app.is_from_sync_and_pending_installation());
  std::vector<apps::IconInfo> icon_infos =
      ParseAppIconInfos("InstallFromSync", web_app.sync_proto().icon_infos())
          .value_or(std::vector<apps::IconInfo>());
  std::optional<SkColor> theme_color;
  if (web_app.sync_proto().has_theme_color()) {
    theme_color = web_app.sync_proto().theme_color();
  }
  InstallFromSyncCommand::Params params = InstallFromSyncCommand::Params(
      web_app.app_id(), web_app.manifest_id(), web_app.start_url(),
      web_app.sync_proto().name(), GURL(web_app.sync_proto().scope()),
      theme_color, web_app.user_display_mode(), icon_infos);
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallFromSyncCommand>(&profile_.get(), params,
                                               std::move(callback)),
      location);
}

void WebAppCommandScheduler::RemoveInstallUrlMaybeUninstall(
    std::optional<webapps::AppId> app_id,
    WebAppManagement::Type install_source,
    const GURL& install_url,
    webapps::WebappUninstallSource uninstall_source,
    UninstallJob::Callback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      WebAppUninstallCommand::CreateForRemoveInstallUrl(
          uninstall_source, *profile_, std::move(app_id), install_source,
          install_url, std::move(callback)),
      location);
}

void WebAppCommandScheduler::RemoveInstallManagementMaybeUninstall(
    const webapps::AppId& app_id,
    WebAppManagement::Type install_source,
    webapps::WebappUninstallSource uninstall_source,
    UninstallJob::Callback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      WebAppUninstallCommand::CreateForRemoveInstallManagements(
          uninstall_source, *profile_, app_id, {install_source},
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::RemoveUserUninstallableManagements(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallJob::Callback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      WebAppUninstallCommand::CreateForRemoveInstallManagements(
          uninstall_source, *profile_, app_id, kUserUninstallableSources,
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::RemoveAllManagementTypesAndUninstall(
    base::PassKey<WebAppSyncBridge>,
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallJob::Callback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      WebAppUninstallCommand::CreateForRemoveInstallManagements(
          uninstall_source, *profile_, app_id, WebAppManagementTypes::All(),
          std::move(callback)),
      location);
}

void WebAppCommandScheduler::UninstallAllUserInstalledWebApps(
    webapps::WebappUninstallSource uninstall_source,
    UninstallAllUserInstalledWebAppsCommand::Callback callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<UninstallAllUserInstalledWebAppsCommand>(
          uninstall_source, *profile_, std::move(callback)),
      location);
}

void WebAppCommandScheduler::SetRunOnOsLoginMode(
    const webapps::AppId& app_id,
    RunOnOsLoginMode login_mode,
    base::OnceClosure callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(app_id, login_mode,
                                                 std::move(callback)),
      location);
}

void WebAppCommandScheduler::SyncRunOnOsLoginMode(
    const webapps::AppId& app_id,
    base::OnceClosure callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSyncLoginMode(app_id, std::move(callback)),
      location);
}

void WebAppCommandScheduler::UpdateProtocolHandlerUserApproval(
    const webapps::AppId& app_id,
    const std::string& protocol_scheme,
    ApiApprovalState approval_state,
    base::OnceClosure callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<UpdateProtocolHandlerApprovalCommand>(
          app_id, protocol_scheme, approval_state, std::move(callback)),
      location);
}

void WebAppCommandScheduler::ClearWebAppBrowsingData(
    const base::Time& begin_time,
    const base::Time& end_time,
    base::OnceClosure done,
    const base::Location& location) {
  provider_->scheduler().ScheduleCallback(
      "ClearWebAppBrowsingData", AllAppsLockDescription(),
      base::BindOnce(web_app::ClearWebAppBrowsingData, begin_time, end_time),
      std::move(done), location);
}

void WebAppCommandScheduler::SetAppIsDisabled(const webapps::AppId& app_id,
                                              bool is_disabled,
                                              base::OnceClosure callback,
                                              const base::Location& location) {
  provider_->scheduler().ScheduleCallback(
      "SetAppIsDisabled", AppLockDescription(app_id),
      base::BindOnce(
          [](const webapps::AppId& app_id, bool is_disabled,
             web_app::AppLock& lock, base::Value::Dict& debug_value) {
            lock.sync_bridge().SetAppIsDisabled(lock, app_id, is_disabled);
          },
          app_id, is_disabled),
      std::move(callback), location);
}

void WebAppCommandScheduler::ComputeAppSize(
    const webapps::AppId& app_id,
    base::OnceCallback<void(std::optional<ComputedAppSize>)> callback) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<ComputeAppSizeCommand>(app_id, &profile_.get(),
                                              std::move(callback)));
}

void WebAppCommandScheduler::LaunchApp(
    const webapps::AppId& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const std::optional<GURL>& url_handler_launch_url,
    const std::optional<GURL>& protocol_handler_launch_url,
    const std::optional<GURL>& file_launch_url,
    const std::vector<base::FilePath>& launch_files,
    LaunchWebAppCallback callback,
    const base::Location& location) {
  LaunchApp(WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
                app_id, command_line, current_directory, url_handler_launch_url,
                protocol_handler_launch_url, file_launch_url, launch_files),
            LaunchWebAppWindowSetting::kOverrideWithWebAppConfig,
            std::move(callback), location);
}

void WebAppCommandScheduler::LaunchApp(const webapps::AppId& app_id,
                                       const std::optional<GURL>& url,
                                       LaunchWebAppCallback callback,
                                       const base::Location& location) {
  CHECK(!url || url->is_valid());
  apps::AppLaunchParams params =
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          app_id, *base::CommandLine::ForCurrentProcess(),
          /*current_directory=*/base::FilePath(),
          /*url_handler_launch_url=*/std::nullopt,
          /*protocol_handler_launch_url=*/std::nullopt,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{});
  params.override_url = url.value_or(GURL());

  LaunchApp(std::move(params),
            LaunchWebAppWindowSetting::kOverrideWithWebAppConfig,
            std::move(callback), location);
}

void WebAppCommandScheduler::LaunchAppWithCustomParams(
    apps::AppLaunchParams params,
    LaunchWebAppCallback callback,
    const base::Location& location) {
  LaunchApp(std::move(params), LaunchWebAppWindowSetting::kUseLaunchParams,
            std::move(callback), location);
}

void WebAppCommandScheduler::InstallAppLocally(const webapps::AppId& app_id,
                                               base::OnceClosure callback,
                                               const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallAppLocallyCommand>(app_id, std::move(callback)),
      location);
}

void WebAppCommandScheduler::SynchronizeOsIntegration(
    const webapps::AppId& app_id,
    base::OnceClosure synchronize_callback,
    std::optional<SynchronizeOsOptions> synchronize_options,
    bool upgrade_to_fully_installed_if_installed,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<OsIntegrationSynchronizeCommand>(
          app_id, synchronize_options, upgrade_to_fully_installed_if_installed,
          std::move(synchronize_callback)),
      location);
}

void WebAppCommandScheduler::SetUserDisplayMode(
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode,
    base::OnceClosure callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<SetUserDisplayModeCommand>(app_id, user_display_mode,
                                                  std::move(callback)),
      location);
}

void WebAppCommandScheduler::ScheduleDedupeInstallUrls(
    base::OnceClosure callback,
    const base::Location& location) {
  base::UmaHistogramCounts100("WebApp.DedupeInstallUrls.SessionRunCount",
                              ++dedupe_install_urls_run_count_);

  provider_->command_manager().ScheduleCommand(
      std::make_unique<DedupeInstallUrlsCommand>(profile_.get(),
                                                 std::move(callback)),
      location);
}

void WebAppCommandScheduler::SetAppCapturesSupportedLinksDisableOverlapping(
    const webapps::AppId app_id,
    bool set_to_preferred,
    base::OnceClosure done,
    const base::Location& location) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED_IN_MIGRATION()
      << "Preferred apps in ChromeOS are implemented in AppService";
#else
  ScheduleCallback(
      "SetAppCapturesSupporedLinks", AllAppsLockDescription(),
      base::BindOnce(::web_app::SetAppCapturesSupportedLinksDisableOverlapping,
                     app_id, set_to_preferred),
      std::move(done), location);
#endif
}

void WebAppCommandScheduler::RunIconDiagnosticsForApp(
    const webapps::AppId& app_id,
    WebAppIconDiagnosticResultCallback result_callback,
    const base::Location& location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<WebAppIconDiagnosticCommand>(&profile_.get(), app_id,
                                                    std::move(result_callback)),
      location);
}

base::WeakPtr<WebAppCommandScheduler> WebAppCommandScheduler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppCommandScheduler::LaunchApp(apps::AppLaunchParams params,
                                       LaunchWebAppWindowSetting option,
                                       LaunchWebAppCallback callback,
                                       const base::Location& location) {
  // Note: Handle the shutdown here, as we have to catch when KeepAlives cannot
  // be created.
  if (IsShuttingDown()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

  auto launch_with_keep_alives =
      base::BindOnce(&WebAppCommandScheduler::LaunchAppWithKeepAlives,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params), option,
                     std::move(callback), std::move(profile_keep_alive),
                     std::move(browser_keep_alive), location);
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
    std::unique_ptr<ScopedKeepAlive> browser_keep_alive,
    const base::Location& location) {
  DCHECK(provider_->is_registry_ready());

  // Decorate the callback to ensure the keep alives are kept alive during the
  // execution of the launch.
  callback = std::move(callback).Then(base::BindOnce(
      [](std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
         std::unique_ptr<ScopedKeepAlive> browser_keep_alive) {},
      std::move(profile_keep_alive), std::move(browser_keep_alive)));

  webapps::AppId app_id = params.app_id;
  provider_->command_manager().ScheduleCommand(
      std::make_unique<LaunchWebAppCommand>(&profile_.get(), provider_.get(),
                                            std::move(params), launch_setting,
                                            std::move(callback)),
      location);
}

bool WebAppCommandScheduler::IsShuttingDown() const {
  return is_in_shutdown_ ||
         KeepAliveRegistry::GetInstance()->IsShuttingDown() ||
         profile_->ShutdownStarted();
}

}  // namespace web_app
