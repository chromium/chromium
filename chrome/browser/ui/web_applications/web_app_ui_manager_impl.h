// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"

class Profile;
class Browser;

namespace web_app {

class WebAppProvider;
class WebAppDialogManager;

// This KeyedService is a UI counterpart for WebAppProvider.
// TODO(calamity): Rename this to WebAppProviderDelegate to better reflect that
// this class serves a wide range of Web Applications <-> Browser purposes.
class WebAppUiManagerImpl : public BrowserListObserver, public WebAppUiManager {
 public:
  static WebAppUiManagerImpl* Get(WebAppProvider* provider);

  explicit WebAppUiManagerImpl(Profile* profile);
  WebAppUiManagerImpl(const WebAppUiManagerImpl&) = delete;
  WebAppUiManagerImpl& operator=(const WebAppUiManagerImpl&) = delete;
  ~WebAppUiManagerImpl() override;

  void SetSubsystems(WebAppSyncBridge* sync_bridge,
                     OsIntegrationManager* os_integration_manager) override;
  void Start() override;
  void Shutdown() override;

  WebAppDialogManager& dialog_manager();

  // WebAppUiManager:
  WebAppUiManagerImpl* AsImpl() override;
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;
  bool UninstallAndReplaceIfExists(const std::vector<AppId>& from_apps,
                                   const AppId& to_app) override;
  bool CanAddAppToQuickLaunchBar() const override;
  void AddAppToQuickLaunchBar(const AppId& app_id) override;
  bool IsAppInQuickLaunchBar(const AppId& app_id) const override;
  bool IsInAppWindow(content::WebContents* web_contents,
                     const AppId* app_id) const override;
  void NotifyOnAssociatedAppChanged(
      content::WebContents* web_contents,
      const absl::optional<AppId>& previous_app_id,
      const absl::optional<AppId>& new_app_id) const override;
  bool CanReparentAppTabToWindow(const AppId& app_id,
                                 bool shortcut_created) const override;
  void ReparentAppTabToWindow(content::WebContents* contents,
                              const AppId& app_id,
                              bool shortcut_created) override;
  void ShowWebAppIdentityUpdateDialog(
      const std::string& app_id,
      bool title_change,
      bool icon_change,
      const std::u16string& old_title,
      const std::u16string& new_title,
      const SkBitmap& old_icon,
      const SkBitmap& new_icon,
      content::WebContents* web_contents,
      web_app::AppIdentityDialogCallback callback) override;

  base::Value LaunchWebApp(apps::AppLaunchParams params,
                           LaunchWebAppWindowSetting launch_setting,
                           Profile& profile,
                           LaunchWebAppCallback callback,
                           AppLock& lock) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

#if BUILDFLAG(IS_WIN)
  // Attempts to uninstall the given web app id. Meant to be used with OS-level
  // uninstallation support/hooks.
  void UninstallWebAppFromStartupSwitch(const AppId& app_id);
#endif

 private:
  // Returns true if Browser is for an installed App.
  bool IsBrowserForInstalledApp(Browser* browser);

  // Returns AppId of the Browser's installed App, |IsBrowserForInstalledApp|
  // must be true.
  AppId GetAppIdForBrowser(Browser* browser);

  void OnExtensionSystemReady();

  void OnShortcutInfoReceivedSearchShortcutLocations(
      const AppId& from_app,
      const AppId& app_id,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  void OnShortcutLocationGathered(const AppId& from_app,
                                  const AppId& app_id,
                                  ShortcutLocations locations);
  void InstallOsHooksForReplacementApp(const AppId& app_id,
                                       ShortcutLocations locations);

  std::unique_ptr<WebAppDialogManager> dialog_manager_;

  const raw_ptr<Profile> profile_;

  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;
  raw_ptr<OsIntegrationManager, DanglingUntriaged> os_integration_manager_ =
      nullptr;

  std::map<AppId, std::vector<base::OnceClosure>> windows_closed_requests_map_;
  std::map<AppId, size_t> num_windows_for_apps_map_;
  bool started_ = false;

  base::WeakPtrFactory<WebAppUiManagerImpl> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
