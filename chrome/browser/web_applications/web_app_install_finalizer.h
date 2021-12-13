// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_system_web_app_data.h"
#include "chrome/browser/web_applications/web_app_uninstall_job.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class WebAppSyncBridge;
class WebAppUiManager;
class WebApp;
class WebAppIconManager;
class WebAppPolicyManager;
class WebAppRegistrar;
class WebAppUninstallJob;
enum class WebAppUninstallJobResult;

// An finalizer for the installation process, represents the last step.
// Takes WebApplicationInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class WebAppInstallFinalizer {
 public:
  using InstallFinalizedCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;
  using UninstallWebAppCallback = base::OnceCallback<void(bool uninstalled)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const AppId& app_id, bool uninstalled)>;

  struct FinalizeOptions {
    FinalizeOptions();
    ~FinalizeOptions();
    FinalizeOptions(const FinalizeOptions&);

    webapps::WebappInstallSource install_source =
        webapps::WebappInstallSource::COUNT;
    bool locally_installed = true;
    bool overwrite_existing_manifest_fields = true;

    absl::optional<WebAppChromeOsData> chromeos_data;
    absl::optional<WebAppSystemWebAppData> system_web_app_data;
    absl::optional<AppId> parent_app_id;
  };

  WebAppInstallFinalizer(Profile* profile,
                         WebAppIconManager* icon_manager,
                         WebAppPolicyManager* policy_manager);
  WebAppInstallFinalizer(const WebAppInstallFinalizer&) = delete;
  WebAppInstallFinalizer& operator=(const WebAppInstallFinalizer&) = delete;
  virtual ~WebAppInstallFinalizer();

  // All methods below are |virtual| for testing.

  // Write the WebApp data to disk and register the app.
  virtual void FinalizeInstall(const WebApplicationInfo& web_app_info,
                               const FinalizeOptions& options,
                               InstallFinalizedCallback callback);

  // Write the new WebApp data to disk and update the app.
  // TODO(https://crbug.com/1196051): Chrome fails to update the manifest
  // if the app window needing update closes at the same time as Chrome.
  // Therefore, the manifest may not always update as expected.
  virtual void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                              InstallFinalizedCallback callback);

  // Removes |webapp_uninstall_source| from |app_id|. If no more interested
  // sources left, deletes the app from disk and registrar.
  virtual void UninstallExternalWebApp(
      const AppId& app_id,
      webapps::WebappUninstallSource external_install_source,
      UninstallWebAppCallback callback);

  // Removes the external app for |app_url| from disk and registrar. Fails if
  // there is no installed external app for |app_url|.
  virtual void UninstallExternalWebAppByUrl(
      const GURL& app_url,
      webapps::WebappUninstallSource webapp_uninstall_source,
      UninstallWebAppCallback callback);

  // Removes |webapp_uninstall_source| from |app_id|. If no more interested
  // sources left, deletes the app from disk and registrar.
  virtual void UninstallWebApp(
      const AppId& app_id,
      webapps::WebappUninstallSource external_install_source,
      UninstallWebAppCallback callback);

  virtual void RetryIncompleteUninstalls(
      const std::vector<AppId>& apps_to_uninstall);

  // Sync-initiated uninstall. Copied from WebAppInstallSyncInstallDelegate.
  // Called before the web apps are removed from the registry by sync. This:
  // * Begins process of uninstalling OS hooks, which initially requires the
  //   registrar to still contain the web app data.
  // * Notifies observers of WebAppWillBeUninstalled.
  // After the app data is fully deleted & os hooks uninstalled:
  // * Notifies observers of WebAppUninstalled.
  // * `callback` is called.
  // The registrar is expected to be synchronously updated after this function
  // call to remove the given `web_apps`.
  virtual void UninstallWithoutRegistryUpdateFromSync(
      const std::vector<AppId>& web_apps,
      RepeatingUninstallCallback callback);

  virtual bool CanUserUninstallWebApp(const AppId& app_id) const;

  // Returns true if the app with |app_id| was previously uninstalled by the
  // user. For example, if a user uninstalls a default app ('default apps' are
  // considered external apps), then this will return true.
  virtual bool WasPreinstalledWebAppUninstalled(const AppId& app_id) const;

  virtual bool CanReparentTab(const AppId& app_id, bool shortcut_created) const;
  virtual void ReparentTab(const AppId& app_id,
                           bool shortcut_created,
                           content::WebContents* web_contents);

  void Start();
  void Shutdown();

  void SetSubsystems(WebAppRegistrar* registrar,
                     WebAppUiManager* ui_manager,
                     WebAppSyncBridge* sync_bridge,
                     OsIntegrationManager* os_integration_manager);

  virtual void SetRemoveSourceCallbackForTesting(
      base::RepeatingCallback<void(const AppId&)>);

  Profile* profile() { return profile_; }

  const WebAppRegistrar& GetWebAppRegistrar() const;

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;

  void UninstallWebAppInternal(const AppId& app_id,
                               webapps::WebappUninstallSource uninstall_source,
                               UninstallWebAppCallback callback);
  void OnUninstallComplete(AppId app_id,
                           webapps::WebappUninstallSource uninstall_source,
                           UninstallWebAppCallback callback,
                           WebAppUninstallJobResult result);
  void UninstallExternalWebAppOrRemoveSource(const AppId& app_id,
                                             Source::Type source,
                                             UninstallWebAppCallback callback);

  void OnMaybeRegisterOsUninstall(const AppId& app_id,
                                  Source::Type source,
                                  UninstallWebAppCallback callback,
                                  OsHooksErrors os_hooks_errors);

  void SetWebAppManifestFieldsAndWriteData(
      const WebApplicationInfo& web_app_info,
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback);

  void OnIconsDataWritten(
      CommitCallback commit_callback,
      std::unique_ptr<WebApp> web_app,
      bool success);

  void OnDatabaseCommitCompletedForInstall(InstallFinalizedCallback callback,
                                           AppId app_id,
                                           bool success);

  bool ShouldUpdateOsHooks(const AppId& app_id);

  void OnDatabaseCommitCompletedForUpdate(
      InstallFinalizedCallback callback,
      AppId app_id,
      std::string old_name,
      bool should_update_os_hooks,
      FileHandlerUpdateAction file_handlers_need_os_update,
      const WebApplicationInfo& web_app_info,
      bool success);

  void OnUpdateHooksFinished(InstallFinalizedCallback callback,
                             AppId app_id,
                             std::string old_name,
                             web_app::OsHooksErrors os_hooks_errors);

  // Returns a value indicating whether the file handlers registered with the OS
  // should be updated. Used to avoid unnecessary updates. TODO(estade): why
  // does this optimization exist when other OS hooks don't have similar
  // optimizations?
  FileHandlerUpdateAction GetFileHandlerUpdateAction(
      const AppId& app_id,
      const WebApplicationInfo& new_web_app_info);

  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;
  raw_ptr<WebAppUiManager> ui_manager_ = nullptr;
  raw_ptr<OsIntegrationManager> os_integration_manager_ = nullptr;

  const raw_ptr<Profile> profile_;
  const raw_ptr<WebAppIconManager> icon_manager_;
  raw_ptr<WebAppPolicyManager> policy_manager_;
  bool started_ = false;

  base::flat_map<AppId, std::unique_ptr<WebAppUninstallJob>>
      pending_uninstalls_;

  base::RepeatingCallback<void(const AppId& app_id)>
      install_source_removed_callback_for_testing_;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
