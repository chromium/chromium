// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_system_web_app_data.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class AppRegistryController;
class FileHandlersPermissionHelper;
class WebAppUiManager;
class WebApp;
class WebAppIconManager;
class WebAppPolicyManager;
class WebAppRegistrar;

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

    absl::optional<WebAppChromeOsData> chromeos_data;
    absl::optional<WebAppSystemWebAppData> system_web_app_data;
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
                              content::WebContents* web_contents,
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

  // Sync-initiated uninstall. Copied from WebAppInstallSyncInstallDelegate.
  // Called before the web apps are removed from the registry. Begins process of
  // uninstalling OS hooks, which initially requires the registrar to still
  // contain the web app data. Also notify observers of WebAppWillBeUninstalled.
  // TODO(dmurph): After migration to WebApp* from the registry, this could
  // potentially just be done in one step, after removal from registry, as os
  // hooks information could be passed.
  virtual void UninstallFromSyncBeforeRegistryUpdate(
      std::vector<AppId> web_apps);
  virtual void UninstallFromSyncAfterRegistryUpdate(
      std::vector<std::unique_ptr<WebApp>> web_apps,
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
                     AppRegistryController* registry_controller,
                     OsIntegrationManager* os_integration_manager);

  virtual void SetRemoveSourceCallbackForTesting(
      base::RepeatingCallback<void(const AppId&)>);

  Profile* profile() { return profile_; }

  WebAppRegistrar& GetWebAppRegistrar() const;

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;
  friend class FileHandlersPermissionHelper;

  // FileHandlersPermissionHelper uses these getters.
  WebAppRegistrar& registrar() const { return *registrar_; }

  AppRegistryController& registry_controller() { return *registry_controller_; }
  OsIntegrationManager& os_integration_manager() {
    return *os_integration_manager_;
  }

  void UninstallWebAppInternal(const AppId& app_id,
                               webapps::WebappUninstallSource uninstall_source,
                               UninstallWebAppCallback callback);
  void UninstallExternalWebAppOrRemoveSource(const AppId& app_id,
                                             Source::Type source,
                                             UninstallWebAppCallback callback);

  void OnSyncUninstallOsHooksUninstall(AppId app_id, OsHooksResults);
  void OnSyncUninstallAppDataDeleted(AppId app_id, bool success);
  // Sync uninstall only finishes once both the hooks are uninstalled
  // (OnSyncUninstallOsHooksUninstall) and app data is deleted
  // (OnSyncUninstallAppDataDeleted).
  void MaybeFinishSyncUninstall(AppId app_id);

  void SetWebAppManifestFieldsAndWriteData(
      const WebApplicationInfo& web_app_info,
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback);

  void OnIconsDataWritten(
      CommitCallback commit_callback,
      std::unique_ptr<WebApp> web_app,
      bool success);

  void OnIconsDataDeletedAndWebAppUninstalled(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      UninstallWebAppCallback callback,
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

  void OnUninstallOsHooks(const AppId& app_id,
                          webapps::WebappUninstallSource uninstall_source,
                          UninstallWebAppCallback callback,
                          OsHooksResults os_hooks_info);

  WebAppRegistrar* registrar_ = nullptr;
  AppRegistryController* registry_controller_ = nullptr;
  WebAppUiManager* ui_manager_ = nullptr;
  OsIntegrationManager* os_integration_manager_ = nullptr;

  Profile* const profile_;
  WebAppIconManager* const icon_manager_;
  WebAppPolicyManager* policy_manager_;
  bool started_ = false;

  struct SyncUninstallState {
    SyncUninstallState();
    ~SyncUninstallState();
    std::unique_ptr<WebApp> web_app;
    UninstallWebAppCallback callback;
    bool hooks_uninstalled = false;
    bool app_data_deleted = false;
    bool success = true;
  };
  base::flat_map<AppId, std::unique_ptr<SyncUninstallState>>
      pending_sync_uninstalls_;

  base::RepeatingCallback<void(const AppId& app_id)>
      install_source_removed_callback_for_testing_;

  std::unique_ptr<FileHandlersPermissionHelper> file_handlers_helper_;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
