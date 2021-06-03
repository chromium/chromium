// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class WebApp;
class WebAppIconManager;
class WebAppRegistrar;

class WebAppInstallFinalizer final : public InstallFinalizer,
                                     public content_settings::Observer {
 public:
  WebAppInstallFinalizer(Profile* profile, WebAppIconManager* icon_manager);
  WebAppInstallFinalizer(const WebAppInstallFinalizer&) = delete;
  WebAppInstallFinalizer& operator=(const WebAppInstallFinalizer&) = delete;
  ~WebAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void FinalizeUninstallAfterSync(const AppId& app_id,
                                  UninstallWebAppCallback callback) override;
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
  bool CanUserUninstallWebApp(const AppId& app_id) const override;
  bool WasPreinstalledWebAppUninstalled(const AppId& app_id) const override;
  void Start() override;
  void Shutdown() override;

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;

  void UninstallWebAppInternal(const AppId& app_id,
                               webapps::WebappUninstallSource uninstall_source,
                               UninstallWebAppCallback callback);
  void UninstallExternalWebAppOrRemoveSource(const AppId& app_id,
                                             Source::Type source,
                                             UninstallWebAppCallback callback);

  void SetWebAppManifestFieldsAndWriteData(
      const WebApplicationInfo& web_app_info,
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback);

  void OnIconsDataWritten(
      CommitCallback commit_callback,
      std::unique_ptr<WebApp> web_app,
      const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
      bool success);

  void OnShortcutsMenuIconsDataWritten(CommitCallback commit_callback,
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
  // TODO(crbug.com/1206036): Replace |should_update_os_hooks| and
  // |file_handlers_need_os_update| with an OsHooksResults bitset to match the
  // granularity we have during install.
  void FinalizeUpdateWithShortcutInfo(
      bool should_update_os_hooks,
      FileHandlerUpdateAction file_handlers_need_os_update,
      InstallFinalizedCallback callback,
      const AppId app_id,
      const WebApplicationInfo& web_app_info,
      std::unique_ptr<ShortcutInfo> old_shortcut);

  bool ShouldUpdateOsHooks(const AppId& app_id);

  // Checks whether OS registered file handlers need to update, taking into
  // account permission settings, as file handlers should be unregistered
  // when the permission has been denied. Also, downgrades granted file handling
  // permissions if file handlers have changed.
  FileHandlerUpdateAction DoFileHandlersNeedOsUpdate(
      const AppId app_id,
      const WebApplicationInfo& web_app_info,
      content::WebContents* web_contents);

  // Resets the FILE_HANDLING content setting permission if `web_app_info` is
  // asking for more file handling types than were previously granted to the
  // app's origin. Returns the new content setting, which will be either
  // unchanged, or will have switched from ALLOW to ASK. If the previous setting
  // was BLOCK, no change is made.
  ContentSetting MaybeResetFileHandlingPermission(
      const WebApplicationInfo& web_app_info);

  void OnDatabaseCommitCompletedForUpdate(
      InstallFinalizedCallback callback,
      AppId app_id,
      std::string old_name,
      std::unique_ptr<ShortcutInfo> old_shortcut,
      bool should_update_os_hooks,
      FileHandlerUpdateAction file_handlers_need_os_update,
      const WebApplicationInfo& web_app_info,
      bool success);

  void OnUninstallOsHooks(const AppId& app_id,
                          webapps::WebappUninstallSource uninstall_source,
                          UninstallWebAppCallback callback,
                          OsHooksResults os_hooks_info);

  WebAppRegistrar& GetWebAppRegistrar() const;

  // content_settings::Observer overrides.
  // This catches permission changes occurring when browser is active, from
  // permission prompts, site settings, and site settings padlock. When
  // permission setting is changed to be blocked/allowed, update app's
  // `file_handler_permission_blocked` state and update file handlers on OS.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // Checks if file handling permission is blocked in settings.
  bool IsFileHandlerPermissionBlocked(const GURL& scope);
  // Update file handler permission state in db and OS.
  void UpdateFileHandlerPermission(const AppId& app_id,
                                   bool permission_blocked);

  // This catches permission changes occurring when browser is not active, like
  // enterprise policy changes. It detects any file handling permission mismatch
  // between the app db state and permission settings, and correct the state in
  // db as well as in OS for all apps during `WebAppInstallFinalizer::Start()`.
  void DetectAndCorrectFileHandlingPermissionBlocks();

  Profile* const profile_;
  WebAppIconManager* const icon_manager_;
  bool started_ = false;

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observer_{this};

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
