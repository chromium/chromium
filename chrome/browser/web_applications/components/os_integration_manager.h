// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_OS_INTEGRATION_MANAGER_H_

#include <bitset>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"
#include "chrome/common/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class AppIconManager;
class WebAppUiManager;

// OsHooksResults contains the result of all Os hook deployments
using OsHooksResults = std::bitset<OsHookType::kMaxValue + 1>;

// Used to pass install options configured from upstream caller.
// All options are disabled by default.
struct InstallOsHooksOptions {
  InstallOsHooksOptions();
  InstallOsHooksOptions(const InstallOsHooksOptions& other);

  OsHooksResults os_hooks;
  bool add_to_desktop = false;
  bool add_to_quick_launch_bar = false;
};

// Callback made after InstallOsHooks is finished.
using InstallOsHooksCallback =
    base::OnceCallback<void(OsHooksResults os_hooks_info)>;

// Callback made after UninstallOsHooks is finished.
using UninstallOsHooksCallback =
    base::OnceCallback<void(OsHooksResults os_hooks_info)>;

// OsIntegrationManager is responsible of creating/updating/deleting
// all OS hooks during Web App lifecycle.
// It contains individual OS integration managers and takes
// care of inter-dependencies among them.
class OsIntegrationManager {
 public:
  explicit OsIntegrationManager(
      Profile* profile,
      std::unique_ptr<AppShortcutManager> shortcut_manager,
      std::unique_ptr<FileHandlerManager> file_handler_manager);
  virtual ~OsIntegrationManager();

  void SetSubsystems(AppRegistrar* registrar,
                     WebAppUiManager* ui_manager,
                     AppIconManager* icon_manager);

  void Start();

  // Install all needed OS hooks for the web app.
  // If provided |web_app_info| is a nullptr, it will read icons data from disk,
  // otherwise it will use (SkBitmaps) from |web_app_info|.
  // virtual for testing
  virtual void InstallOsHooks(const AppId& app_id,
                              InstallOsHooksCallback callback,
                              std::unique_ptr<WebApplicationInfo> web_app_info,
                              InstallOsHooksOptions options);

  // Uninstall specific OS hooks for the web app.
  // Used when removing specific hooks resulting from an app setting change.
  // Example: Running on OS login.
  // TODO(https://crbug.com/1108109) we should record uninstall result and allow
  // callback. virtual for testing
  virtual void UninstallOsHooks(const AppId& app_id,
                                const OsHooksResults& os_hooks,
                                UninstallOsHooksCallback callback);

  // Uninstall all OS hooks for the web app.
  // Used when uninstalling a web app.
  // virtual for testing
  virtual void UninstallAllOsHooks(const AppId& app_id,
                                   UninstallOsHooksCallback callback);

  // Update all needed OS hooks for the web app.
  // virtual for testing
  virtual void UpdateOsHooks(const AppId& app_id,
                             base::StringPiece old_name,
                             const WebApplicationInfo& web_app_info);

  // Proxy calls for AppShortcutManager.
  bool CanCreateShortcuts() const;
  void GetShortcutInfoForApp(
      const AppId& app_id,
      AppShortcutManager::GetShortcutInfoCallback callback);

  // Proxy calls for FileHandlerManager.
  bool IsFileHandlingAPIAvailable(const AppId& app_id);
  const apps::FileHandlers* GetEnabledFileHandlers(const AppId& app_id);
  const base::Optional<GURL> GetMatchingFileHandlerURL(
      const AppId& app_id,
      const std::vector<base::FilePath>& launch_files);
  void MaybeUpdateFileHandlingOriginTrialExpiry(
      content::WebContents* web_contents,
      const AppId& app_id);
  void ForceEnableFileHandlingOriginTrial(const AppId& app_id);
  void DisableForceEnabledFileHandlingOriginTrial(const AppId& app_id);

  // Getter for testing FileHandlerManager
  FileHandlerManager& file_handler_manager_for_testing();

  void SuppressOsHooksForTesting();

 protected:
  AppShortcutManager* shortcut_manager() { return shortcut_manager_.get(); }
  FileHandlerManager* file_handler_manager() {
    return file_handler_manager_.get();
  }
  void set_shortcut_manager(
      std::unique_ptr<AppShortcutManager> shortcut_manager) {
    shortcut_manager_ = std::move(shortcut_manager);
  }
  void set_file_handler_manager(
      std::unique_ptr<FileHandlerManager> file_handler_manager) {
    file_handler_manager_ = std::move(file_handler_manager);
  }

 private:
  void OnShortcutsCreated(const AppId& app_id,
                          std::unique_ptr<WebApplicationInfo> web_app_info,
                          InstallOsHooksOptions options,
                          base::RepeatingCallback<void(OsHookType::Type os_hook,
                                                       bool created)> callback,
                          bool shortcuts_created);

  void RegisterRunOnOsLogin(const AppId& app_id,
                            RegisterRunOnOsLoginCallback callback);

  void OnShortcutInfoRetrievedRegisterRunOnOsLogin(
      RegisterRunOnOsLoginCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  void DeleteSharedAppShims(const AppId& app_id);

  Profile* const profile_;
  AppRegistrar* registrar_ = nullptr;
  WebAppUiManager* ui_manager_ = nullptr;

  std::unique_ptr<AppShortcutManager> shortcut_manager_;
  std::unique_ptr<FileHandlerManager> file_handler_manager_;

  bool suppress_os_hooks_for_testing_ = false;
  base::WeakPtrFactory<OsIntegrationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_OS_INTEGRATION_MANAGER_H_
