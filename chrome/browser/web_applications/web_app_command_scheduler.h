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
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/commands/internal/callback_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"

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

namespace base {
class Time;
}  // namespace base

class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace web_app {

class ComputedAppSizeWithOrigin;
class IsolatedWebAppInstallSource;
class IsolatedWebAppUrlInfo;
class IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo;
class IsolationData;
class SignedWebBundleMetadata;
class WebApp;
class WebAppProvider;
enum class ApiApprovalState;
enum class ApplyPendingManifestUpdateResult;
enum class FallbackBehavior;
enum class InstallableCheckResult;
enum class IsolatedInstallabilityCheckResult;
enum class LaunchWebAppWindowSetting;
enum class RunOnOsLoginMode;
enum class ManifestUpdateCheckResult;
enum class ManifestUpdateResult;
enum class ManifestSilentUpdateCheckResult;
enum class NavigateAndTriggerInstallDialogCommandResult;
struct CleanupOrphanedIsolatedWebAppsCommandError;
struct CleanupOrphanedIsolatedWebAppsCommandSuccess;
struct ExternalInstallOptions;
struct ExternallyManagedAppManagerInstallResult;
struct InstallIsolatedWebAppCommandError;
struct InstallIsolatedWebAppCommandSuccess;
struct IsolatedWebAppUpdatePrepareAndStoreCommandError;
struct IsolatedWebAppUpdatePrepareAndStoreCommandSuccess;
struct SynchronizeOsOptions;
struct WebAppIconDiagnosticResult;
struct WebAppInstallInfo;
struct ManifestSilentUpdateCompletionInfo;
enum class FetchManifestAndUpdateResult;

#if BUILDFLAG(IS_CHROMEOS)
class CleanupBundleCacheSuccess;
class CleanupBundleCacheError;
class CopyBundleToCacheSuccess;
enum class CopyBundleToCacheError;
class GetBundleCachePathSuccess;
enum class GetBundleCachePathError;
class RemoveObsoleteBundleVersionsError;
class RemoveObsoleteBundleVersionsSuccess;
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

  // Starts a user-initiated installation process from the given `WebContents`.
  // This is triggered by a user action, like clicking an install icon in the
  // omnibox. It fetches the manifest, shows an install dialog, and if the user
  // accepts, it proceeds with the installation. The `fallback_behavior`
  // determines what happens if the site is not fully installable (e.g. has no
  // manifest).
  void FetchManifestAndInstall(webapps::WebappInstallSource install_surface,
                               base::WeakPtr<content::WebContents> contents,
                               WebAppInstallDialogCallback dialog_callback,
                               OnceInstallCallback callback,
                               FallbackBehavior behavior,
                               const base::Location& location = FROM_HERE);

  // Fetches the `WebAppInstallInfo` for a given `install_url`. This is used
  // for installing sub-apps, where the `manifest_id` and optional
  // `parent_manifest_id` are known beforehand.
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

  // Installs a web app from a pre-filled `WebAppInstallInfo` struct, bypassing
  // the manifest fetching step. This is for programmatic installations where
  // the app's metadata is already known. This version does not install any OS
  // hooks and is primarily for testing.
  void InstallFromInfoNoIntegrationForTesting(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      OnceInstallCallback install_callback,
      const base::Location& location = FROM_HERE);

  // Similar to `InstallFromInfoNoIntegrationForTesting`, but allows specifying
  // `WebAppInstallParams` to control how OS integration (like shortcuts and
  // run-on-os-login) is configured.
  void InstallFromInfoWithParams(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      OnceInstallCallback install_callback,
      const WebAppInstallParams& install_params,
      const base::Location& location = FROM_HERE);

  using ExternalInstallCallback =
      base::OnceCallback<void(ExternallyManagedAppManagerInstallResult)>;
  // Installs a web app from an external source, like a policy, default app, or
  // system component. This handles loading the install URL, fetching the
  // manifest, and creating the web app. It can also install a placeholder if
  // the full installation fails, and can replace existing placeholders if
  // specified in `external_install_options`.
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
  // Checks if an installed web app has an updated manifest. It fetches the new
  // manifest from the app's `url`, compares it with the existing one, and if
  // there are changes, it may prompt the user for confirmation before applying
  // them.
  void ScheduleManifestUpdateCheck(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> contents,
      ManifestUpdateCheckCompletedCallback callback,
      const base::Location& location = FROM_HERE);

  using ManifestSilentUpdateCompletedCallback = base::OnceCallback<void(
      ManifestSilentUpdateCompletionInfo completion_info)>;
  // A newer version of `ScheduleManifestUpdateCheck` that uses a more
  // predictable app updating algorithm. This will eventually replace the
  // original.
  // For more details, go/predictable-app-updating-design-doc.
  void ScheduleManifestSilentUpdate(
      content::WebContents& contents,
      std::optional<base::Time> previous_time_for_silent_icon_update,
      ManifestSilentUpdateCompletedCallback callback,
      const base::Location& location = FROM_HERE);

  using ApplyPendingManifestUpdateCallback =
      base::OnceCallback<void(ApplyPendingManifestUpdateResult check_result)>;
  // Applies any stored pending update metadata to the web app, updating its
  // security sensitive fields in accordance to a more predictable app updating
  // algorithm as defined in go/predictable-app-updating-design-doc.
  void ScheduleApplyPendingManifestUpdate(
      const webapps::AppId& app_id,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ApplyPendingManifestUpdateCallback callback,
      const base::Location& location = FROM_HERE);

  // Finalizes a manifest update by writing the new `install_info` to the
  // database. This is often called after all app windows are closed to avoid
  // conflicts. The keep-alives ensure the browser doesn't shut down during the
  // write. `install_info` must be non-null.
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
  // Checks if a URL is installable as a web app, used for enterprise policy
  // checks. Returns whether it's installable, not installable, or already
  // installed, along with the app ID if applicable.
  void FetchInstallabilityForChromeManagement(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      FetchInstallabilityForChromeManagementCallback callback,
      const base::Location& location = FROM_HERE);

  // The navigation will always succeed. The `result` indicates whether the
  // command was able to trigger the install dialog. This opens a new tab,
  // navigates to `install_url`, and if the site is installable, it triggers
  // the install dialog for the user.
  using NavigateAndTriggerInstallDialogCommandCallback =
      base::OnceCallback<void(
          NavigateAndTriggerInstallDialogCommandResult result)>;
  void ScheduleNavigateAndTriggerInstallDialog(
      const GURL& install_url,
      const GURL& origin_url,
      bool is_renderer_initiated,
      NavigateAndTriggerInstallDialogCommandCallback callback,
      const base::Location& location = FROM_HERE);

  // Installs an Isolated Web App from the given `url_info` and
  // `install_source`. If `expected_version` is set, this command will refuse to
  // install the Isolated Web App if its version does not match.
  virtual void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppInstallSource& install_source,
      const std::optional<IwaVersion>& expected_version,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      InstallIsolatedWebAppCallback callback,
      const base::Location& call_location = FROM_HERE);

  // Finds and removes any Isolated Web App data directories on disk that are
  // no longer referenced by an installed app in the WebAppRegistrar. This can
  // happen if the browser crashes during IWA installation or uninstallation.
  virtual void CleanupOrphanedIsolatedApps(
      CleanupOrphanedIsolatedWebAppsCallback callback,
      const base::Location& call_location = FROM_HERE);

  using PrepareAndStoreIsolatedWebAppUpdateCallback = base::OnceCallback<void(
      base::expected<IsolatedWebAppUpdatePrepareAndStoreCommandSuccess,
                     IsolatedWebAppUpdatePrepareAndStoreCommandError>)>;
  // Prepares an update for an Isolated Web App. `update_info` specifies the
  // location of the update for the IWA referred to in `url_info`. This command
  // is safe to run even if the IWA is not installed or already updated. If a
  // dry-run of the update succeeds, then the `update_info` is persisted in the
  // `IsolationData::pending_update_info()` of the IWA in the Web App database.
  virtual void PrepareAndStoreIsolatedWebAppUpdate(
      const IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo& update_info,
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      PrepareAndStoreIsolatedWebAppUpdateCallback callback,
      const base::Location& call_location = FROM_HERE);

  // Applies a prepared pending update to an Isolated Web App. This command is
  // safe to run even if the IWA is not installed or already updated. Regardless
  // of whether the update succeeds or fails,
  // `IsolationData::pending_update_info` of the IWA in the Web App database
  // will be cleared.
  virtual void ApplyPendingIsolatedWebAppUpdate(
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<void(IsolatedWebAppApplyUpdateCommandResult)> callback,
      const base::Location& call_location = FROM_HERE);

  // Checks if a Signed Web Bundle is a valid and installable Isolated Web App.
  // It compares the version from the bundle's metadata with an already
  // installed app (if one exists) to determine if the bundle is a new install,
  // an update, or outdated.
  virtual void CheckIsolatedWebAppBundleInstallability(
      const SignedWebBundleMetadata& bundle_metadata,
      base::OnceCallback<void(IsolatedInstallabilityCheckResult,
                              std::optional<IwaVersion>)> callback,
      const base::Location& call_location = FROM_HERE);

#if BUILDFLAG(IS_CHROMEOS)
  // Gets the path to an IWA bundle in the cache for a given `session_type`. If
  // `version` is not provided, it returns the path to the newest cached
  // version.
  void GetIsolatedWebAppBundleCachePath(
      const IsolatedWebAppUrlInfo& url_info,
      const std::optional<IwaVersion>& version,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(
          base::expected<GetBundleCachePathSuccess, GetBundleCachePathError>)>
          callback,
      const base::Location& call_location = FROM_HERE);

  // Copies an IWA bundle file to the cache for a given `session_type`.
  void CopyIsolatedWebAppBundleToCache(
      const IsolatedWebAppUrlInfo& url_info,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(base::expected<CopyBundleToCacheSuccess,
                                             CopyBundleToCacheError>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Cleans all IWA cached bundles for a given `session_type` that are not in
  // the `iwas_to_keep_in_cache` list.
  void CleanupIsolatedWebAppBundleCache(
      const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(
          base::expected<CleanupBundleCacheSuccess, CleanupBundleCacheError>)>
          callback,
      const base::Location& call_location = FROM_HERE);

  void RemoveObsoleteIsolatedWebAppVersionsCache(
      const IsolatedWebAppUrlInfo& url_info,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<
          void(base::expected<RemoveObsoleteBundleVersionsSuccess,
                              RemoveObsoleteBundleVersionsError>)> callback,
      const base::Location& call_location = FROM_HERE);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Calculates the total browsing data size for all installed Isolated Web
  // Apps.
  void GetIsolatedWebAppBrowsingData(
      base::OnceCallback<void(base::flat_map<url::Origin, uint64_t>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Gets the StoragePartitionConfig for a <controlledframe> within the given
  // Isolated Web App. If the partition is persistent (not `in_memory`), it is
  // registered in the WebAppProvider.
  void GetControlledFramePartition(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name,
      bool in_memory,
      base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
          callback,
      const base::Location& location = FROM_HERE);

  // Installs a web app using data from a sync update. It first tries to fetch a
  // live manifest from the app's start URL. If that fails, it falls back to
  // using the information from the sync data to ensure the app is installed.
  void InstallFromSync(const WebApp& web_app,
                       OnceInstallCallback callback,
                       const base::Location& location = FROM_HERE);

  // Removes an `install_url` associated with a given `install_source` for an
  // app. If `app_id` is not provided, it will act on the first matching app.
  // If this is the last install URL for that source, the source is removed. If
  // it's the last source for the app, the app is uninstalled.
  //
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

  // Removes an install management source from a given web app. If this is the
  // last source for the app, the app is uninstalled. This also disconnects any
  // sub-apps and uninstalls them if they have no other install sources.
  //
  // Note: This may cause a web app to become user-uninstallable. In that case,
  // it will deploy uninstall OS hooks to ensure that it can be uninstallable
  // via the OS (windows control panel -> apps -> uninstall).
  virtual void RemoveInstallManagementMaybeUninstall(
      const webapps::AppId& app_id,
      WebAppManagement::Type install_management,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCallback callback,
      const base::Location& location = FROM_HERE);

  // Removes all management types that a user can uninstall. If the app was
  // installed by default, it will be added to the
  // `UserUninstalledPreinstalledWebAppPrefs`.
  //
  // Note: This may cause a web app to become user-uninstallable. In that case,
  // it will deploy uninstall OS hooks to ensure that it can be uninstallable
  // via the OS.
  void RemoveUserUninstallableManagements(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCallback callback,
      const base::Location& location = FROM_HERE);

  using UninstallAllUserInstalledWebAppsCallback =
      base::OnceCallback<void(const std::optional<std::string>& error_message)>;
  // Uninstalls all web apps that were installed by the user.
  void UninstallAllUserInstalledWebApps(
      webapps::WebappUninstallSource uninstall_source,
      UninstallAllUserInstalledWebAppsCallback callback,
      const base::Location& location = FROM_HERE);

  // Completely removes the web app from the database by removing all management
  // types. This is a destructive operation and should be used with caution.
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

  // Sets whether the web app should run on OS login, according to `login_mode`.
  void SetRunOnOsLoginMode(const webapps::AppId& app_id,
                           RunOnOsLoginMode login_mode,
                           base::OnceClosure callback,
                           const base::Location& location = FROM_HERE);

  // Syncs the run-on-OS-login mode from the web app DB to the OS.
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

  // Sets the app to disabled. This is ChromeOS-specific and a no-op on other
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

  // Clears web app-specific browsing data (like last launch time and badging
  // time) within the given time range.
  void ClearWebAppBrowsingData(const base::Time& begin_time,
                               const base::Time& end_time,
                               base::OnceClosure done,
                               const base::Location& location = FROM_HERE);

  // Launches the given app. This call also uses keep-alives to guarantee that
  // the browser and profile will not destruct before the launch is complete.
  void LaunchApp(const webapps::AppId& app_id,
                 const base::CommandLine& command_line,
                 const base::FilePath& current_directory,
                 const std::optional<GURL>& protocol_handler_launch_url,
                 const std::optional<GURL>& file_launch_url,
                 const std::vector<base::FilePath>& launch_files,
                 LaunchWebAppCallback callback,
                 const base::Location& location = FROM_HERE);

  // Launches the given app to the given url if specified, or the app
  // `start_url` if not. This uses keep-alives to guarantee the
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

  // Takes an app that is already in the registry (e.g. from sync) and installs
  // it with OS integration, making it available in the launcher, on the
  // desktop, etc.
  void InstallAppLocally(const webapps::AppId& app_id,
                         base::OnceClosure callback,
                         const base::Location& location = FROM_HERE);

  // Forces a synchronization of a web app's OS integration state with the
  // database. If `upgrade_to_fully_installed_if_installed` is true and the app
  // is installed, this command will upgrade the
  // installation status to proto::InstallState::INSTALLED_WITH_OS_INTEGRATION.
  void SynchronizeOsIntegration(
      const webapps::AppId& app_id,
      base::OnceClosure synchronize_callback,
      std::optional<SynchronizeOsOptions> synchronize_options = std::nullopt,
      bool upgrade_to_fully_installed_if_installed = false,
      const base::Location& location = FROM_HERE);

  // Sets the user's preferred display mode for an app (e.g., window vs. tab).
  // This also ensures OS integration is triggered if the new display mode is
  // one that requires it (i.e. anything other than "browser").
  void SetUserDisplayMode(const webapps::AppId& app_id,
                          mojom::UserDisplayMode user_display_mode,
                          base::OnceClosure callback,
                          const base::Location& location = FROM_HERE);

#if BUILDFLAG(IS_MAC)
  // Rewrites icons for an app if and only if it is a DIY app, where this
  // operation has not yet occurred. This will set
  // `WebApp::diy_app_icons_masked_on_mac()` to true when
  // complete.
  void RewriteDiyIcons(const webapps::AppId& app_id,
                       base::OnceCallback<void(RewriteIconResult)> callback,
                       const base::Location& location = FROM_HERE);
#endif  // BUILDFLAG(IS_MAC)

  // Finds web apps that share the same install URLs (possibly across different
  // install sources) and dedupes the install URL configs into the most
  // recently installed non-placeholder-like web app. See crbug.com/1427340.
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

  // Runs a series of icon health checks for `app_id`. See
  // `WebAppIconDiagnosticResult` for more information on what diagnostics are
  // returned.
  void RunIconDiagnosticsForApp(
      const webapps::AppId& app_id,
      WebAppIconDiagnosticResultCallback result_callback,
      const base::Location& location = FROM_HERE);

  // Implements the Web Install API (`navigator.install()`).
  // Calls `installed_callback` with the `InstallResultCode` and the computed
  // manifest id if successful. Used by Web Install API.
  void InstallAppFromUrl(const GURL& install_url,
                         const std::optional<GURL>& manifest_id,
                         base::WeakPtr<content::WebContents> web_contents,
                         const GURL& last_committed_url,
                         WebAppInstallDialogCallback dialog_callback,
                         WebInstallFromUrlCommandCallback installed_callback,
                         const base::Location& location = FROM_HERE);

  // Feches the install_url, validates that an installable manifest with a
  // manifest id exists and matches the given one. Then, locks the app lock for
  // the app and and updates the app if it is installed. This assumes it is a
  // trusted update, so trusted icons are copied from all manifest icons.
  //
  // Note: Callers may want to check if the app is installed first before
  // calling this to not waste resources loading the install url in the
  // background.
  void FetchManifestAndUpdate(
      const GURL& install_url,
      const webapps::ManifestId& manifest_id,
      base::OnceCallback<void(FetchManifestAndUpdateResult)> callback,
      const base::Location& location = FROM_HERE);

  base::WeakPtr<WebAppCommandScheduler> GetWeakPtr();

  // Safely gets all apps given the WebAppFilter.
  void GetAllAppsForFilter(
      const WebAppFilter&,
      base::OnceCallback<void(std::vector<webapps::AppId>)> callback);

  // Synchronizes the os integration of all apps that apply to the filter.
  void SynchronizeOsIntegrationForAllApps(const WebAppFilter& filter,
                                          base::OnceClosure callback);

  // Reads pending app update information like icons to show on the dialog from
  // disk, and uses that with web app metadata to construct a
  // WebAppIdentityUpdate instance.
  void ReadAppUpdateDataFromDisk(
      const webapps::AppId& app_id,
      base::OnceCallback<void(std::optional<WebAppIdentityUpdate>)> callback,
      const base::Location& location = FROM_HERE);

  // Marks whether the pending update available for the app is ignored by the
  // user, and notifies changes to the WebAppRegistrar.
  void MarkAppPendingUpdateAsIgnored(
      const webapps::AppId& app_id,
      base::OnceClosure done,
      const base::Location& location = FROM_HERE);

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
