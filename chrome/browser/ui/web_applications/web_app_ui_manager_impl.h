// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"

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
  static WebAppUiManagerImpl* Get(Profile* profile);

  explicit WebAppUiManagerImpl(Profile* profile);
  WebAppUiManagerImpl(const WebAppUiManagerImpl&) = delete;
  WebAppUiManagerImpl& operator=(const WebAppUiManagerImpl&) = delete;
  ~WebAppUiManagerImpl() override;

  void SetSubsystems(AppRegistryController* app_registry_controller,
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
  bool IsInAppWindow(content::WebContents* web_contents,
                     const AppId* app_id) const override;
  void NotifyOnAssociatedAppChanged(content::WebContents* web_contents,
                                    const AppId& previous_app_id,
                                    const AppId& new_app_id) const override;
  bool CanReparentAppTabToWindow(const AppId& app_id,
                                 bool shortcut_created) const override;
  void ReparentAppTabToWindow(content::WebContents* contents,
                              const AppId& app_id,
                              bool shortcut_created) override;
  content::WebContents* NavigateExistingWindow(const AppId& app_id,
                                               const GURL& url) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

#if defined(OS_WIN)
  // Attempts to uninstall the given web app id. Meant to be used with OS-level
  // uninstallation support/hooks.
  void UninstallWebAppFromStartupSwitch(const AppId& app_id);
#endif

 private:
  // Returns true if Browser is for an installed App.
  bool IsBrowserForInstalledApp(Browser* browser);

  // Returns AppId of the Browser's installed App, |IsBrowserForInstalledApp|
  // must be true.
  const AppId GetAppIdForBrowser(Browser* browser);

  void OnShortcutInfoReceivedSearchShortcutLocations(
      const AppId& from_app,
      const AppId& app_id,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  void OnShortcutLocationGathered(const AppId& from_app,
                                  const AppId& app_id,
                                  ShortcutLocations locations);

  std::unique_ptr<WebAppDialogManager> dialog_manager_;

  Profile* const profile_;

  AppRegistryController* app_registry_controller_ = nullptr;
  OsIntegrationManager* os_integration_manager_ = nullptr;

  std::map<AppId, std::vector<base::OnceClosure>> windows_closed_requests_map_;
  std::map<AppId, size_t> num_windows_for_apps_map_;
  bool started_ = false;

  base::WeakPtrFactory<WebAppUiManagerImpl> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
