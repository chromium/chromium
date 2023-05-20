// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_

#include "base/containers/flat_map.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/manifest_update_check_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/commands/navigate_and_trigger_install_dialog_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"

class GURL;
class Profile;

struct WebAppInstallInfo;

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace web_app {

class IsolatedWebAppUrlInfo;
class WebAppDataRetriever;
class WebAppProvider;
struct IsolationData;
class WebApp;
enum class ApiApprovalState;
struct SynchronizeOsOptions;

// The command scheduler is the main API to access the web app system. The
// scheduler internally ensures:
// * Operations occur after the WebAppProvider is ready (so you don't have to
//   manually wait for this).
// * Operations are isolated from other operations in the system (currently
//   implemented using `WebAppCommand`s) to prevent race conditions while
//   reading/writing from the various data storage of the system.
// * Operations have the necessary dependencies from the WebAppProvider system.
class WebAppCommandScheduler {
 public:
  using ManifestWriteCallback =
      ManifestUpdateFinalizeCommand::ManifestWriteCallback;
  using InstallIsolatedWebAppCallback = base::OnceCallback<void(
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError>)>;

  WebAppCommandScheduler(Profile& profile, WebAppProvider* provider);
  virtual ~WebAppCommandScheduler();

  void Shutdown();

  // User initiated install that uses current `WebContents` to fetch manifest
  // and install the web app.
  void FetchManifestAndInstall(webapps::WebappInstallSource install_surface,
                               base::WeakPtr<content::WebContents> contents,
                               bool bypass_service_worker_check,
                               WebAppInstallDialogCallback dialog_callback,
                               OnceInstallCallback callback,
                               bool use_fallback,
                               const base::Location& location = FROM_HERE);

  // Install with provided `WebAppInstallInfo` instead of fetching data from
  // manifest.
  // `InstallFromInfo` doesn't install OS hooks. `InstallFromInfoWithParams`
  // install OS hooks when they are set in `install_params`.
  void InstallFromInfo(std::unique_ptr<WebAppInstallInfo> install_info,
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

  void InstallFromInfoWithParams(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      base::OnceCallback<void(const AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace)> install_callback,
      const WebAppInstallParams& install_params,
      const std::vector<AppId>& apps_to_uninstall,
      const base::Location& location = FROM_HERE);

  // Install web apps managed by `ExternallyInstalledAppManager`.
  void InstallExternallyManagedApp(
      const ExternalInstallOptions& external_install_options,
      base::OnceCallback<void(const AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace)> install_callback,
      base::WeakPtr<content::WebContents> contents,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      WebAppUrlLoader* web_app_url_loader,
      const base::Location& location = FROM_HERE);

  // Install a placeholder app, this is used during externally managed install
  // flow when url load fails.
  void InstallPlaceholder(
      const ExternalInstallOptions& install_options,
      base::OnceCallback<void(const AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace)> callback,
      base::WeakPtr<content::WebContents> web_contents,
      const base::Location& location = FROM_HERE);

  void PersistFileHandlersUserChoice(
      const AppId& app_id,
      bool allowed,
      base::OnceClosure callback,
      const base::Location& location = FROM_HERE);

  // Schedule a command that performs fetching data from the manifest
  // for a manifest update.
  void ScheduleManifestUpdateCheck(
      const GURL& url,
      const AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> contents,
      ManifestUpdateCheckCommand::CompletedCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that performs the data writes into the DB for
  // completion of the manifest update.
  void ScheduleManifestUpdateFinalize(
      const GURL& url,
      const AppId& app_id,
      WebAppInstallInfo install_info,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ManifestWriteCallback callback,
      const base::Location& location = FROM_HERE);

  void FetchInstallabilityForChromeManagement(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      FetchInstallabilityForChromeManagementCallback callback,
      const base::Location& location = FROM_HERE);

  void ScheduleNavigateAndTriggerInstallDialog(
      const GURL& install_url,
      const GURL& origin_url,
      bool is_renderer_initiated,
      NavigateAndTriggerInstallDialogCommandCallback callback,
      const base::Location& location = FROM_HERE);

  // Schedules a command that installs the Isolated Web App described by the
  // given IsolatedWebAppUrlInfo and IsolationData.
  virtual void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppLocation& location,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      InstallIsolatedWebAppCallback callback,
      const base::Location& call_location = FROM_HERE);

  // Computes the browsing data size of all installed Isolated Web Apps.
  void GetIsolatedWebAppBrowsingData(
      base::OnceCallback<void(base::flat_map<url::Origin, int64_t>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Registers a <controlledframe>'s StoragePartition with the given Isolated
  // Web App.
  void RegisterControlledFramePartition(
      const AppId& app_id,
      const std::string& partition_name,
      base::OnceClosure callback,
      const base::Location& location = FROM_HERE);

  // Scheduler a command that installs a web app from sync.
  void InstallFromSync(const WebApp& web_app,
                       OnceInstallCallback callback,
                       const base::Location& location = FROM_HERE);

  // Schedules a command that uninstalls a web app.
  void Uninstall(const AppId& app_id,
                 absl::optional<WebAppManagement::Type> external_install_source,
                 webapps::WebappUninstallSource uninstall_source,
                 WebAppInstallFinalizer::UninstallWebAppCallback callback,
                 const base::Location& location = FROM_HERE);

  // Schedules a command that updates run on os login to provided `login_mode`
  // for a web app.
  void SetRunOnOsLoginMode(const AppId& app_id,
                           RunOnOsLoginMode login_mode,
                           base::OnceClosure callback,
                           const base::Location& location = FROM_HERE);

  // Schedules a command that syncs the run on os login mode from web app DB to
  // OS.
  void SyncRunOnOsLoginMode(const AppId& app_id,
                            base::OnceClosure callback,
                            const base::Location& location = FROM_HERE);

  // Updates the approved or disallowed protocol list for the given app. If
  // necessary, it also updates the protocol registration with the OS.
  void UpdateProtocolHandlerUserApproval(
      const AppId& app_id,
      const std::string& protocol_scheme,
      ApiApprovalState approval_state,
      base::OnceClosure callback,
      const base::Location& location = FROM_HERE);

  // Set app to disabled, This is Chrome OS specific and no-op on other
  // platforms.
  void SetAppIsDisabled(const AppId& app_id,
                        bool is_disabled,
                        base::OnceClosure callback,
                        const base::Location& location = FROM_HERE);

  // Schedules provided callback after `lock` is granted. The callback can
  // access web app resources through the `lock`. The `operation_name` is used
  // describe this operation in the WebAppCommandManager log, surfaced in
  // chrome://web-app-internals for debugging purposes.
  // If the system is shutting down, or has already shut down, then the callback
  // will not be called & will simply be destroyed.
  template <typename LockType,
            typename DescriptionType = typename LockType::LockDescription>
  void ScheduleCallbackWithLock(
      const std::string& operation_name,
      std::unique_ptr<DescriptionType> lock_description,
      base::OnceCallback<void(LockType& lock)> callback,
      const base::Location& location = FROM_HERE);
  // Same as above, but the callback can return a debug value to also be used in
  // WebAppCommandManager logs, viewable from chrome://web-app-internals.
  template <typename LockType,
            typename DescriptionType = typename LockType::LockDescription>
  void ScheduleCallbackWithLock(
      const std::string& operation_name,
      std::unique_ptr<DescriptionType> lock_description,
      base::OnceCallback<base::Value(LockType& lock)> callback,
      const base::Location& location = FROM_HERE);

  // Schedules to clear the browsing data for web app, given the inclusive time
  // range.
  void ClearWebAppBrowsingData(const base::Time& begin_time,
                               const base::Time& end_time,
                               base::OnceClosure done,
                               const base::Location& location = FROM_HERE);

  // Launches the given app. This call also uses keep-alives to guarantee that
  // the browser and profile will not destruct before the launch is complete.
  void LaunchApp(const AppId& app_id,
                 const base::CommandLine& command_line,
                 const base::FilePath& current_directory,
                 const absl::optional<GURL>& url_handler_launch_url,
                 const absl::optional<GURL>& protocol_handler_launch_url,
                 const absl::optional<GURL>& file_launch_url,
                 const std::vector<base::FilePath>& launch_files,
                 LaunchWebAppCallback callback,
                 const base::Location& location = FROM_HERE);

  // Used to launch apps with a custom launch params. This does not respect the
  // configuration of the app, and will respect whatever the params say.
  void LaunchAppWithCustomParams(apps::AppLaunchParams params,
                                 LaunchWebAppCallback callback,
                                 const base::Location& location = FROM_HERE);

  // Used to locally install an app from the chrome://apps page, triggered
  // by the AppLauncherHandler.
  void InstallAppLocally(const AppId& app_id,
                         base::OnceClosure callback,
                         const base::Location& location = FROM_HERE);

  // Used to schedule a synchronization of a web app's OS states with the
  // current DB states.
  void SynchronizeOsIntegration(
      const AppId& app_id,
      base::OnceClosure synchronize_callback,
      absl::optional<SynchronizeOsOptions> synchronize_options = absl::nullopt,
      const base::Location& location = FROM_HERE);

  // TODO(https://crbug.com/1298130): expose all commands for web app
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
  // Safe because we live on the WebAppProvider.
  raw_ptr<WebAppProvider, DanglingUntriaged> provider_;

  bool is_in_shutdown_ = false;
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  base::WeakPtrFactory<WebAppCommandScheduler> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
