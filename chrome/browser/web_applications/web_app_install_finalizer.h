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
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class FileHandlersPermissionHelper;
class WebApp;
class WebAppIconManager;
class WebAppPolicyManager;
class WebAppRegistrar;

class WebAppInstallFinalizer final : public InstallFinalizer {
 public:
  WebAppInstallFinalizer(Profile* profile,
                         WebAppIconManager* icon_manager,
                         WebAppPolicyManager* policy_manager);
  WebAppInstallFinalizer(const WebAppInstallFinalizer&) = delete;
  WebAppInstallFinalizer& operator=(const WebAppInstallFinalizer&) = delete;
  ~WebAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                      content::WebContents* web_contents,
                      InstallFinalizedCallback callback) override;

  void UninstallExternalWebApp(
      const AppId& app_id,
      webapps::WebappUninstallSource external_install_source,
      UninstallWebAppCallback callback) override;

  void UninstallWebApp(const AppId& app_id,
                       webapps::WebappUninstallSource external_install_source,
                       UninstallWebAppCallback callback) override;

  void UninstallFromSyncBeforeRegistryUpdate(
      std::vector<AppId> web_apps) override;
  void UninstallFromSyncAfterRegistryUpdate(
      std::vector<std::unique_ptr<WebApp>> web_apps,
      RepeatingUninstallCallback callback) override;

  bool CanUserUninstallWebApp(const AppId& app_id) const override;
  bool WasPreinstalledWebAppUninstalled(const AppId& app_id) const override;
  void Start() override;
  void Shutdown() override;

  void SetRemoveSourceCallbackForTesting(
      base::RepeatingCallback<void(const AppId&)>) override;

  Profile* profile() { return profile_; }

  WebAppRegistrar& GetWebAppRegistrar() const;

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;
  friend class FileHandlersPermissionHelper;

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
