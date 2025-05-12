// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/commands/internal/callback_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class GURL;
class Profile;
class Browser;

namespace content {
class StoragePartitionConfig;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace web_app {

class ComputedAppSizeWithOrigin;
class IsolatedWebAppInstallSource;
class IsolatedWebAppUrlInfo;
class IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo;
class IsolatedWebAppApplyUpdateCommandSuccess;
class IsolationData;
class SignedWebBundleMetadata;
class WebApp;
class WebAppProvider;
enum class ApiApprovalState;
enum class FallbackBehavior;
enum class InstallableCheckResult;
enum class IsolatedInstallabilityCheckResult;
enum class LaunchWebAppWindowSetting;
enum class RunOnOsLoginMode;
enum class ManifestUpdateCheckResult;
enum class ManifestUpdateResult;
enum class NavigateAndTriggerInstallDialogCommandResult;
struct CleanupOrphanedIsolatedWebAppsCommandError;
struct CleanupOrphanedIsolatedWebAppsCommandSuccess;
struct ExternalInstallOptions;
struct ExternallyManagedAppManagerInstallResult;
struct InstallIsolatedWebAppCommandError;
struct InstallIsolatedWebAppCommandSuccess;
struct IsolatedWebAppApplyUpdateCommandError;
struct IsolatedWebAppUpdatePrepareAndStoreCommandError;
struct IsolatedWebAppUpdatePrepareAndStoreCommandSuccess;
struct SynchronizeOsOptions;
struct WebAppIconDiagnosticResult;
struct WebAppInstallInfo;

#if BUILDFLAG(IS_CHROMEOS)
class CleanupBundleCacheSuccess;
class CleanupBundleCacheError;
class CopyBundleToCacheSuccess;
enum class CopyBundleToCacheError;
class GetBundleCachePathSuccess;
enum class GetBundleCachePathError;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
enum class RewriteIconResult;
#endif  // BUILDFLAG(IS_MAC)
// The command scheduler is the main API to access the web app system. The
// scheduler internally ensures:
// * Operations occur after the WebAppProvider is ready (so you don't have to
//   manually wait for this).
// * Operations are isolated from other operations in the system (currently
//   implemented using `WebAppCommand`s) to prevent race conditions while
//   reading/writing from the various data storage of the system.
// * Operations have the necessary dependencies from the WebAppProvider system.
//
// Note: When adding new commands to this scheduler, please avoid including them
// in this file, and instead forward declare needed types above.
class WebAppCommandScheduler {
 public:
  using ManifestWriteCallback =
      base::OnceCallback<void(const GURL& url,
                              const webapps::AppId& app_id,
                              ManifestUpdateResult result)>;
  using InstallIsolatedWebAppCallback = base::OnceCallback<void(
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError>)>;
  using CleanupOrphanedIsolatedWebAppsCallback = base::OnceCallback<void(
      base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                     CleanupOrphanedIsolatedWebAppsCommandError>)>;
  using WebAppIconDiagnosticResultCallback =
      base::OnceCallback<void(std::optional<WebAppIconDiagnosticResult>)>;
  using WebInstallFromUrlCommandCallback =
      base::OnceCallback<void(const webapps::AppId& app_id,
                              webapps::InstallResultCode code)>;
  using UninstallCallback =
      base::OnceCallback<void(webapps::UninstallResultCode)>;
  using LaunchWebAppCallback =
      base::OnceCallback<void(base::WeakPtr<Browser> browser,
                              base::WeakPtr<content::WebContents> web_contents,
                              apps::LaunchContainer container)>;
  using LaunchWebAppDebugValueCallback =
      base::OnceCallback<void(base::WeakPtr<Browser> browser,
                              base::WeakPtr<content::WebContents> web_contents,
                              apps::LaunchContainer container,
                              base::Value debug_value)>;

  explicit WebAppCommandScheduler(Profile& profile);
  virtual ~WebAppCommandScheduler();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Shutdown();

  // User initiated install that uses current `WebContents` to fetch manifest
  // and install the web app.
  void FetchManifestAndInstall(webapps::WebappInstallSource install_surface,
                               base::WeakPtr<content::WebContents> contents,
                               WebAppInstallDialogCallback dialog_callback,
                               OnceInstallCallback callback,
                               FallbackBehavior behavior,
                               const base::Location& location = FROM_HERE);

  void FetchInstallInfoFromInstallUrl(
      webapps::ManifestId manifest_id,
      GURL install_url,
      webapps::ManifestId parent_manifest_id,
      base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback);

  // Same as the overload above, but without parent_manifest_id.
  void FetchInstallInfoFromInstallUrl(
      webapps::ManifestId manifest_id,
      GURL install_url,
      base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback);

  // Install with provided `WebAppInstallInfo` instead of fetching data from
  // manifest.
  // `InstallFromInfo` doesn't install OS hooks. `InstallFromInfoWithParams`
  // install OS hooks when they are set in `install_params`.
  void InstallFromInfoNoIntegrationForTesting(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      OnceInstallCallback install_callback,
      const base::Location& location = FROM_HERE);

  void InstallFromInfoWithParams(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      OnceInstallCallback install_callback,
      const WebAppInstallParams& install_params,
      const base::Location& location = FROM_HERE);

  using ExternalInstallCallback =
      base::OnceCallback<void(ExternallyManagedAppManagerInstallResult)>;
  // Install web apps managed by `ExternallyInstalledAppManager`.
  void InstallExternallyManagedApp(
      const ExternalInstallOptions& external_install_options,
      std::optional<webapps::AppId> installed_placeholder_app_id,
      ExternalInstallCallback installed_callback,
      const base::Location& location = FROM_HERE);

  void PersistFileHandlersUserChoice(
      const webapps::AppId& app_id,
      bool allowed,
      base::OnceClosure callback,
      const base::Location& location = FROM_HERE);

  using ManifestUpdateCheckCompletedCallback = base::OnceCallback<void(
      ManifestUpdateCheckResult check_result,
      std::unique_ptr<WebAppInstallInfo> new_install_info)>;
  // Schedule a command that performs fetching data from the manifest
  // for a manifest update.
  void ScheduleManifestUpdateCheck(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> contents,
      ManifestUpdateCheckCompletedCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedule a command that performs fetching data from the manifest
  // for a manifest update. This is part of the predicatable app updating
  // algorithm that will be implemented. After implementation, this should
  // replace the current ScheduleManifestUpdateCheck.
  // For more details, go/predictable-app-updating-design-doc.
  void ScheduleManifestUpdateCheckV2(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> contents,
      ManifestUpdateCheckCompletedCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that performs the data writes into the DB for
  // completion of the manifest update. `install_info` must be non-null.
  void ScheduleManifestUpdateFinalize(
      const GURL& url,
      const webapps::AppId& app_id,
      std::unique_ptr<WebAppInstallInfo> install_info,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ManifestWriteCallback callback,
      const base::Location& location = FROM_HERE);

  using FetchInstallabilityForChromeManagementCallback =
      base::OnceCallback<void(InstallableCheckResult result,
                              std::optional<webapps::AppId> app_id)>;
  void FetchInstallabilityForChromeManagement(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      FetchInstallabilityForChromeManagementCallback callback,
      const base::Location& location = FROM_HERE);

  // The navigation will always succeed. The `result` indicates whether the
  // command was able to trigger the install dialog.
  using NavigateAndTriggerInstallDialogCommandCallback =
      base::OnceCallback<void(
          NavigateAndTriggerInstallDialogCommandResult result)>;
  void ScheduleNavigateAndTriggerInstallDialog(
      const GURL& install_url,
      const GURL& origin_url,
      bool is_renderer_initiated,
      NavigateAndTriggerInstallDialogCommandCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that installs the Isolated Web App described by the
  // given IsolatedWebAppUrlInfo and IsolationData. If `expected_version` is
  // set, then this command will refuse to install the Isolated Web App if its
  // version does not match.
  virtual void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppInstallSource& install_source,
      const std::optional<base::Version>& expected_version,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      InstallIsolatedWebAppCallback callback,
      const base::Location& call_location = FROM_HERE);

  virtual void CleanupOrphanedIsolatedApps(
      CleanupOrphanedIsolatedWebAppsCallback callback,
      const base::Location& call_location = FROM_HERE);

  using PrepareAndStoreIsolatedWebAppUpdateCallback = base::OnceCallback<void(
      base::expected<IsolatedWebAppUpdatePrepareAndStoreCommandSuccess,
                     IsolatedWebAppUpdatePrepareAndStoreCommandError>)>;
  // Schedules a command to prepare the update of an Isolated Web App.
  // `update_info` specifies the location of the update for the IWA referred to
  // in `url_info`. This command is safe to run even if the IWA is not installed
  // or already updated, in which case it will gracefully fail. If a dry-run of
  // the update succeeds, then the `update_info` is persisted in the
  // `IsolationData::pending_update_info()` of the IWA in the Web App database.
  virtual void PrepareAndStoreIsolatedWebAppUpdate(
      const IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo& update_info,
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      PrepareAndStoreIsolatedWebAppUpdateCallback callback,
      const base::Location& call_location = FROM_HERE);

  // Schedules a command to apply a prepared pending update of an Isolated Web
  // App. This command is safe to run even if the IWA is not installed or
  // already updated, in which case it will gracefully fail. Regardless of
  // whether the update succeeds or fails, `IsolationData::pending_update_info`
  // of the IWA in the Web App database will be cleared.
  virtual void ApplyPendingIsolatedWebAppUpdate(
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<
          void(base::expected<IsolatedWebAppApplyUpdateCommandSuccess,
                              IsolatedWebAppApplyUpdateCommandError>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Given the |bundle_metadata| of a Signed Web Bundle, schedules a command to
  // check the installability of the bundle.
  virtual void CheckIsolatedWebAppBundleInstallability(
      const SignedWebBundleMetadata& bundle_metadata,
      base::OnceCallback<void(IsolatedInstallabilityCheckResult,
                              std::optional<base::Version>)> callback,
      const base::Location& call_location = FROM_HERE);

#if BUILDFLAG(IS_CHROMEOS)
  // Schedules a command that gets IWA bundle path from cache for
  // `session_type`. If `version` is not provided, returns the newest cached
  // version.
  void GetIsolatedWebAppBundleCachePath(
      const IsolatedWebAppUrlInfo& url_info,
      const std::optional<base::Version>& version,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(
          base::expected<GetBundleCachePathSuccess, GetBundleCachePathError>)>
          callback,
      const base::Location& call_location = FROM_HERE);

  //  Schedules a command that copies IWA bundle file to the cache for
  //  `session_type`.
  void CopyIsolatedWebAppBundleToCache(
      const IsolatedWebAppUrlInfo& url_info,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(base::expected<CopyBundleToCacheSuccess,
                                             CopyBundleToCacheError>)> callback,
      const base::Location& call_location = FROM_HERE);

  //  Schedules a command that cleans all IWA cached bundles for `session_type`
  //  which are not in the `iwas_to_keep_in_cache`.
  void CleanupIsolatedWebAppBundleCache(
      const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(
          base::expected<CleanupBundleCacheSuccess, CleanupBundleCacheError>)>
          callback,
      const base::Location& call_location = FROM_HERE);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Computes the browsing data size of all installed Isolated Web Apps.
  void GetIsolatedWebAppBrowsingData(
      base::OnceCallback<void(base::flat_map<url::Origin, uint64_t>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Registers a <controlledframe>'s StoragePartition with the given Isolated
  // Web App.
  void GetControlledFramePartition(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name,
      bool in_memory,
      base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
          callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that installs a web app from sync.
  void InstallFromSync(const WebApp& web_app,
                       OnceInstallCallback callback,
                       const base::Location& location = FROM_HERE);

  // Schedules a command that removes `install_source`'s `install_url` from
  // `app_id`, if `app_id` is unset then the first matching web app that has
  // `install_url` for `install_source` will be used.
  // This will remove the install source if there are no remaining install URLs
  // for that install source which in turn will remove the web app if there are
  // no remaining install sources for the web app.
  // Virtual for testing.
  // TODO(crbug.com/40264854): There could potentially be multiple app matches
  // for `install_source` and `install_url` when `app_id` is not provided,
  // handle this case better than "first matching".
  virtual void RemoveInstallUrlMaybeUninstall(
      std::optional<webapps::AppId> app_id,
      WebAppManagement::Type install_source,
      const GURL& install_url,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that removes an install sources from a given web app.
  // This will uninstall the web app if no install sources remain. This also
  // disconnects it from any of its sub apps and uninstalls them too if they
  // have no other install sources.
  //
  // Notes: This may cause a web app to become user uninstallable. In that case
  // it will deploy uninstall OS hooks to ensure that it can be uninstallable
  // via the OS (windows control panel -> apps -> uninstall).
  virtual void RemoveInstallManagementMaybeUninstall(
      const webapps::AppId& app_id,
      WebAppManagement::Type install_management,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCallback callback,
      const base::Location& location = FROM_HERE);

  // Removes all management types that the user can remove, adds the
  // uninstall web app to `UserUninstalledPreinstalledWebAppPrefs` if it was
  // `kDefault` installed. Will CHECK-fail if `uninstall_source` is not
  // `webapps::IsUserUninstall`.
  //
  // Notes: This may cause a web app to become user uninstallable. In that case
  // it will deploy uninstall OS hooks to ensure that it can be uninstallable
  // via the OS.
  void RemoveUserUninstallableManagements(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCallback callback,
      const base::Location& location = FROM_HERE);

  using UninstallAllUserInstalledWebAppsCallback =
      base::OnceCallback<void(const std::optional<std::string>& error_message)>;
  // Schedules a command that uninstalls all user-installed web apps.
  void UninstallAllUserInstalledWebApps(
      webapps::WebappUninstallSource uninstall_source,
      UninstallAllUserInstalledWebAppsCallback callback,
      const base::Location& location = FROM_HERE);

  // Completely removes the web_app from the database by removing all management
  // types. Since this is a very destructive operation, prefer invoking
  // RemoveInstallUrlMaybeUninstall(), RemoveInstallManagementMaybeUninstall(),
  // RemoveUserUninstallableManagements() or UninstallAllUserInstalledWebApps()
  // instead.
  // Currently, only the WebAppSyncBridge is allowed to invoke this for
  // uninstalling web apps, since it is safe to assume that apps marked with
  // `is_uninstalling` set to true can be fully removed from the registry.
  void RemoveAllManagementTypesAndUninstall(
      base::PassKey<WebAppSyncBridge>,
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that updates run on os login to provided `login_mode`
  // for a web app.
  void SetRunOnOsLoginMode(const webapps::AppId& app_id,
                           RunOnOsLoginMode login_mode,
                           base::OnceClosure callback,
                           const base::Location& location = FROM_HERE);

  // Schedules a command that syncs the run on os login mode from web app DB to
  // OS.
  void SyncRunOnOsLoginMode(const webapps::AppId& app_id,
                            base::OnceClosure callback,
                            const base::Location& location = FROM_HERE);

  // Updates the approved or disallowed protocol list for the given app. If
  // necessary, it also updates the protocol registration with the OS.
  void UpdateProtocolHandlerUserApproval(
      const webapps::AppId& app_id,
      const std::string& protocol_scheme,
      ApiApprovalState approval_state,
      base::OnceClosure callback,
      const base::Location& location = FROM_HERE);

  // Set app to disabled, This is Chrome OS specific and no-op on other
  // platforms.
  void SetAppIsDisabled(const webapps::AppId& app_id,
                        bool is_disabled,
                        base::OnceClosure callback,
                        const base::Location& location = FROM_HERE);

  // Schedules a command that calculates the app and data size of a web app.
  void ComputeAppSize(
      const webapps::AppId& app_id,
      base::OnceCallback<void(std::optional<ComputedAppSizeWithOrigin>)>
          callback);

  // The command callback type for `ScheduleCallback*`.
  // - `lock`: This provides access to read & write parts of the WebAppProvider
  //   system. See WebAppCommand for information on locks, and you can find them
  //   in chrome/browser/ewb_applications/locks.
  // - `debug_value`: This can be populated with helpful debugging information,
  //   and will visible in production in chrome://web-app-internals.
  template <typename LockType, typename ReturnType>
  using CallbackCommand =
      base::OnceCallback<ReturnType(LockType& lock,
                                    base::Value::Dict& debug_value)>;
  // `ScheduleCallback*` methods provide convenient way to do operations
  // on the WebAppProvider system that don't require any async work, but still
  // have all of the safety guarantees of commands. All require a:
  // - A command callback to do the operation with the lock.
  // - A completion callback that is called after the command callback is
  //   complete (and accepts the command return value as an argument).
  // The completion callback is guaranteed to be called, even on system
  // shutdown.
  //
  // `ScheduleCallback` is the equivalent of base::PostTaskAndReply for the
  // command system.
  template <typename LockType>
  void ScheduleCallback(const std::string& operation_name,
                        LockType::LockDescription lock_description,
                        CallbackCommand<LockType, void> callback,
                        base::OnceClosure on_complete,
                        const base::Location& location = FROM_HERE) {
    provider_->command_manager().ScheduleCommand(
        std::make_unique<internal::CallbackCommand<LockType>>(
            operation_name, std::move(lock_description), std::move(callback),
            std::move(on_complete)),
        location);
  }

  // `ScheduleCallbackWithResult` is the equivalent of
  // base::PostTaskAndReplyWithResult for the command system.
  template <typename LockType,
            typename CompletionCallbackArg,
            typename CallbackReturnValue = std::decay_t<CompletionCallbackArg>>
  void ScheduleCallbackWithResult(
      const std::string& operation_name,
      LockType::LockDescription lock_description,
      CallbackCommand<LockType, CallbackReturnValue> callback,
      base::OnceCallback<void(CompletionCallbackArg)> on_complete,
      CallbackReturnValue arg_for_shutdown,
      const base::Location& location = FROM_HERE) {
    provider_->command_manager().ScheduleCommand(
        std::make_unique<internal::CallbackCommandWithResult<
            LockType, CompletionCallbackArg>>(
            operation_name, std::move(lock_description), std::move(callback),
            std::move(on_complete), std::move(arg_for_shutdown)),
        location);
  }

  // Schedules to clear the browsing data for web app, given the inclusive time
  // range.
  void ClearWebAppBrowsingData(const base::Time& begin_time,
                               const base::Time& end_time,
                               base::OnceClosure done,
                               const base::Location& location = FROM_HERE);

  // Launches the given app. This call also uses keep-alives to guarantee that
  // the browser and profile will not destruct before the launch is complete.
  void LaunchApp(const webapps::AppId& app_id,
                 const base::CommandLine& command_line,
                 const base::FilePath& current_directory,
                 const std::optional<GURL>& url_handler_launch_url,
                 const std::optional<GURL>& protocol_handler_launch_url,
                 const std::optional<GURL>& file_launch_url,
                 const std::vector<base::FilePath>& launch_files,
                 LaunchWebAppCallback callback,
                 const base::Location& location = FROM_HERE);

  // Launches the given app to the given url if specified, or the app
  // `start_url` if not specified. This uses keep-alives to guarantee the
  // browser and profile stay alive. Will CHECK-fail if `url` is not valid.
  void LaunchApp(const webapps::AppId& app_id,
                 const std::optional<GURL>& url,
                 LaunchWebAppCallback callback,
                 const base::Location& location = FROM_HERE);

  // Used to launch apps with a custom launch params. This does not respect the
  // configuration of the app, and will respect whatever the params say. If you
  // are launching an app, you likely do NOT want to use this method.
  void LaunchAppWithCustomParams(apps::AppLaunchParams params,
                                 LaunchWebAppCallback callback,
                                 const base::Location& location = FROM_HERE);

  // Used to locally install an app from the chrome://apps page, triggered
  // by the AppLauncherHandler.
  void InstallAppLocally(const webapps::AppId& app_id,
                         base::OnceClosure callback,
                         const base::Location& location = FROM_HERE);

  // Used to schedule a synchronization of a web app's OS states with the
  // current DB states. If `upgrade_to_fully_installed_if_installed` is
  // specified and the app is installed, then this command will upgrade the
  // installation status to proto::InstallState::INSTALLED_WITH_OS_INTEGRATION.
  void SynchronizeOsIntegration(
      const webapps::AppId& app_id,
      base::OnceClosure synchronize_callback,
      std::optional<SynchronizeOsOptions> synchronize_options = std::nullopt,
      bool upgrade_to_fully_installed_if_installed = false,
      const base::Location& location = FROM_HERE);

  // Sets the user display mode for an app, and also makes sure os integration
  // is triggered if the new user display mode is one that requires that (i.e.
  // anything other than "browser").
  void SetUserDisplayMode(const webapps::AppId& app_id,
                          mojom::UserDisplayMode user_display_mode,
                          base::OnceClosure callback,
                          const base::Location& location = FROM_HERE);

#if BUILDFLAG(IS_MAC)
  // Rewrites icons for an app if and only if it is a DIY app, where this
  // operation has not yet occurred (e.g. WebApp::diy_app_icons_masked_on_mac()
  // returns false). This will set diy_app_icons_masked_on_mac() to true when
  // complete.
  void RewriteDiyIcons(const webapps::AppId& app_id,
                       base::OnceCallback<void(RewriteIconResult)> callback,
                       const base::Location& location = FROM_HERE);
#endif  // BUILDFLAG(IS_MAC)

  // Finds web apps that share the same install URLs (possibly across different
  // install sources) and dedupes the install URL configs into the most
  // recently installed non-placeholder-like web app.
  // Placeholder-like web apps are either marked as placeholder or have
  // their name set to their start URL like a placeholder. This is an erroneous
  // state some web apps have gotten into, see https://crbug.com/1427340.
  void ScheduleDedupeInstallUrls(base::OnceClosure callback,
                                 const base::Location& location = FROM_HERE);

  // Sets the user preference for link capturing for the given app. If
  // `set_to_preferred` is true, then links in the browser can be launched in
  // the app corresponding to app_id, respecting the app's launch handler
  // preferences. Additionally, if there are multiple apps within the same
  // scope, this will reset the preference on those apps to false.
  void SetAppCapturesSupportedLinksDisableOverlapping(
      const webapps::AppId app_id,
      bool set_to_preferred,
      base::OnceClosure done,
      const base::Location& location = FROM_HERE);

  // Runs a series of icon health checks for |app_id|. Look into
  // |WebAppIconDiagnosticResult| for more information on what icon diagnostics
  // are returned by this command.
  void RunIconDiagnosticsForApp(
      const webapps::AppId& app_id,
      WebAppIconDiagnosticResultCallback result_callback,
      const base::Location& location = FROM_HERE);

  // User initiated install uses the shared web contents to install the content
  // at `install_url`, with optional `manifest_id`.
  // Calls `installed_callback` with the `InstallResultCode` and the computed
  // manifest id if successful. Used by Web Install API.
  void InstallAppFromUrl(const GURL& install_url,
                         const std::optional<GURL>& manifest_id,
                         base::WeakPtr<content::WebContents> web_contents,
                         WebAppInstallDialogCallback dialog_callback,
                         WebInstallFromUrlCommandCallback installed_callback,
                         const base::Location& location = FROM_HERE);

  base::WeakPtr<WebAppCommandScheduler> GetWeakPtr();

  // TODO(crbug.com/40215411): expose all commands for web app
  // operations.

 private:
  void LaunchApp(apps::AppLaunchParams params,
                 LaunchWebAppWindowSetting option,
                 LaunchWebAppCallback callback,
                 const base::Location& location);

  void LaunchAppWithKeepAlives(
      apps::AppLaunchParams params,
      LaunchWebAppWindowSetting option,
      LaunchWebAppCallback callback,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      std::unique_ptr<ScopedKeepAlive> browser_keep_alive,
      const base::Location& location);

  bool IsShuttingDown() const;

  const raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  bool is_in_shutdown_ = false;

  // Track how many times ScheduleDedupeInstallUrls() is invoked for metrics to
  // check that it's not happening excessively.
  // TODO(crbug.com/40264854): Remove once validating that the numbers look okay
  // out in the wild.
  size_t dedupe_install_urls_run_count_ = 0;

  base::WeakPtrFactory<WebAppCommandScheduler> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
