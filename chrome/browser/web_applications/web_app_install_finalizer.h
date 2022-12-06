// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class WebAppSyncBridge;
class WebAppUiManager;
class WebApp;
class WebAppIconManager;
class WebAppInstallManager;
class WebAppPolicyManager;
class WebAppRegistrar;
class WebAppTranslationManager;
class WebAppCommandManager;

// An finalizer for the installation process, represents the last step.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class WebAppInstallFinalizer {
 public:
  using InstallFinalizedCallback =
      base::OnceCallback<void(const AppId& app_id,
                              webapps::InstallResultCode code,
                              OsHooksErrors os_hooks_errors)>;
  using UninstallWebAppCallback =
      base::OnceCallback<void(webapps::UninstallResultCode code)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const AppId& app_id,
                                   webapps::UninstallResultCode code)>;

  struct FinalizeOptions {
    explicit FinalizeOptions(webapps::WebappInstallSource install_surface);
    ~FinalizeOptions();
    FinalizeOptions(const FinalizeOptions&);

    const WebAppManagement::Type source;
    const webapps::WebappInstallSource install_surface;
    bool locally_installed = true;
    bool overwrite_existing_manifest_fields = true;
    bool skip_icon_writes_on_download_failure = false;

    absl::optional<WebAppChromeOsData> chromeos_data;
    absl::optional<ash::SystemWebAppData> system_web_app_data;
    absl::optional<AppId> parent_app_id;
    absl::optional<web_app::IsolationData> isolation_data;

    // If true, OsIntegrationManager::InstallOsHooks won't be called at all,
    // meaning that all other OS Hooks related parameters below will be ignored.
    bool bypass_os_hooks = false;

    // These OS shortcut fields can't be true if |locally_installed| is false.
    // They only have an effect when |bypass_os_hooks| is false.
    bool add_to_applications_menu = true;
    bool add_to_desktop = true;
    bool add_to_quick_launch_bar = true;
  };

  explicit WebAppInstallFinalizer(Profile* profile);
  WebAppInstallFinalizer(const WebAppInstallFinalizer&) = delete;
  WebAppInstallFinalizer& operator=(const WebAppInstallFinalizer&) = delete;
  virtual ~WebAppInstallFinalizer();

  // All methods below are |virtual| for testing.

  // Write the WebApp data to disk and register the app.
  virtual void FinalizeInstall(const WebAppInstallInfo& web_app_info,
                               const FinalizeOptions& options,
                               InstallFinalizedCallback callback);

  // Write the new WebApp data to disk and update the app.
  // TODO(https://crbug.com/1196051): Chrome fails to update the manifest
  // if the app window needing update closes at the same time as Chrome.
  // Therefore, the manifest may not always update as expected.
  virtual void FinalizeUpdate(const WebAppInstallInfo& web_app_info,
                              InstallFinalizedCallback callback);

  // Removes |webapp_uninstall_surface| from |app_id|. If no more interested
  // sources left, deletes the app from disk and registrar.
  virtual void UninstallExternalWebApp(
      const AppId& app_id,
      WebAppManagement::Type external_install_source,
      webapps::WebappUninstallSource uninstall_surface,
      UninstallWebAppCallback callback);

  // Removes the external app for |app_url| from disk and registrar. Fails if
  // there is no installed external app for |app_url|.
  virtual void UninstallExternalWebAppByUrl(
      const GURL& app_url,
      WebAppManagement::Type external_install_source,
      webapps::WebappUninstallSource uninstall_surface,
      UninstallWebAppCallback callback);

  // Removes |webapp_uninstall_surface| from |app_id|, no matter how many
  // sources are left.
  virtual void UninstallWebApp(const AppId& app_id,
                               webapps::WebappUninstallSource uninstall_surface,
                               UninstallWebAppCallback callback);

  virtual bool CanUserUninstallWebApp(const AppId& app_id) const;

  virtual bool CanReparentTab(const AppId& app_id, bool shortcut_created) const;
  virtual void ReparentTab(const AppId& app_id,
                           bool shortcut_created,
                           content::WebContents* web_contents);

  void Start();
  void Shutdown();

  void SetSubsystems(WebAppInstallManager* install_manager,
                     WebAppRegistrar* registrar,
                     WebAppUiManager* ui_manager,
                     WebAppSyncBridge* sync_bridge,
                     OsIntegrationManager* os_integration_manager,
                     WebAppIconManager* icon_manager,
                     WebAppPolicyManager* policy_manager,
                     WebAppTranslationManager* translation_manager,
                     WebAppCommandManager* command_manager);

  virtual void SetRemoveManagementTypeCallbackForTesting(
      base::RepeatingCallback<void(const AppId&)>);

  Profile* profile() { return profile_; }

  const WebAppRegistrar& GetWebAppRegistrar() const;

  // Writes external config data to the web_app DB, mapped per source.
  void WriteExternalConfigMapInfo(WebApp& web_app,
                                  WebAppManagement::Type source,
                                  bool is_placeholder,
                                  GURL install_url);

  // Used to schedule a WebAppUninstallCommand. The |external_install_source|
  // field is only required for external app uninstalls to verify OS
  // unregistration, and is not used for sync/manual uninstalls.
  void ScheduleUninstallCommand(
      const AppId& app_id,
      absl::optional<WebAppManagement::Type> external_install_source,
      webapps::WebappUninstallSource uninstall_source,
      UninstallWebAppCallback callback);

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;

  void OnMaybeRegisterOsUninstall(const AppId& app_id,
                                  WebAppManagement::Type source,
                                  UninstallWebAppCallback callback,
                                  OsHooksErrors os_hooks_errors);

  void SetWebAppManifestFieldsAndWriteData(
      const WebAppInstallInfo& web_app_info,
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback,
      bool skip_icon_writes_on_download_failure);

  void WriteTranslations(
      const AppId& app_id,
      const base::flat_map<std::string, blink::Manifest::TranslationItem>&
          translations,
      CommitCallback commit_callback,
      bool success);

  void CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                          CommitCallback commit_callback,
                          bool success);

  void OnDatabaseCommitCompletedForInstall(InstallFinalizedCallback callback,
                                           AppId app_id,
                                           FinalizeOptions finalize_options,
                                           bool success);

  void OnInstallHooksFinished(InstallFinalizedCallback callback,
                              AppId app_id,
                              OsHooksErrors os_hooks_errors);
  void NotifyWebAppInstalledWithOsHooks(AppId app_id);

  bool ShouldUpdateOsHooks(const AppId& app_id);

  void OnDatabaseCommitCompletedForUpdate(
      InstallFinalizedCallback callback,
      AppId app_id,
      std::string old_name,
      FileHandlerUpdateAction file_handlers_need_os_update,
      const WebAppInstallInfo& web_app_info,
      bool success);

  void OnUpdateHooksFinished(InstallFinalizedCallback callback,
                             AppId app_id,
                             std::string old_name,
                             OsHooksErrors os_hooks_errors);

  // Returns a value indicating whether the file handlers registered with the OS
  // should be updated. Used to avoid unnecessary updates. TODO(estade): why
  // does this optimization exist when other OS hooks don't have similar
  // optimizations?
  FileHandlerUpdateAction GetFileHandlerUpdateAction(
      const AppId& app_id,
      const WebAppInstallInfo& new_web_app_info);

  raw_ptr<WebAppInstallManager, DanglingUntriaged> install_manager_ = nullptr;
  raw_ptr<WebAppRegistrar, DanglingUntriaged> registrar_ = nullptr;
  raw_ptr<WebAppSyncBridge, DanglingUntriaged> sync_bridge_ = nullptr;
  raw_ptr<WebAppUiManager, DanglingUntriaged> ui_manager_ = nullptr;
  raw_ptr<OsIntegrationManager, DanglingUntriaged> os_integration_manager_ =
      nullptr;
  raw_ptr<WebAppIconManager, DanglingUntriaged> icon_manager_ = nullptr;
  raw_ptr<WebAppPolicyManager, DanglingUntriaged> policy_manager_ = nullptr;
  raw_ptr<WebAppTranslationManager, DanglingUntriaged> translation_manager_ =
      nullptr;
  raw_ptr<WebAppCommandManager, DanglingUntriaged> command_manager_ = nullptr;

  const raw_ptr<Profile> profile_;
  bool started_ = false;

  base::RepeatingCallback<void(const AppId& app_id)>
      management_type_removed_callback_for_testing_;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
