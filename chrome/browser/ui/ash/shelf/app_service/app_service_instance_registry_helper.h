// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "components/services/app_service/public/cpp/instance.h"

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

class AppServiceAppWindowShelfController;

// The helper class to operate the App Service Instance Registry.
class AppServiceInstanceRegistryHelper {
 public:
  explicit AppServiceInstanceRegistryHelper(
      AppServiceAppWindowShelfController* controller);

  AppServiceInstanceRegistryHelper(const AppServiceInstanceRegistryHelper&) =
      delete;
  AppServiceInstanceRegistryHelper& operator=(
      const AppServiceInstanceRegistryHelper&) = delete;

  ~AppServiceInstanceRegistryHelper();

  void ActiveUserChanged();
  void AdditionalUserAddedToSession();

  // Notifies the AppService InstanceRegistry that active tabs are changed.
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents);

  // Notifies the AppService InstanceRegistry that the tab's contents are
  // changed. The |old_contents|'s instance will be removed, and the
  // |new_contents|'s instance will be added.
  void OnTabReplaced(content::WebContents* old_contents,
                     content::WebContents* new_contents);

  // Notifies the AppService InstanceRegistry that a new tab is inserted. A new
  // instance will be add tp App Service InstanceRegistry.
  void OnTabInserted(content::WebContents* contents);

  // Notifies the AppService InstanceRegistry that the tab is closed. The
  // instance will be removed from App Service InstanceRegistry.
  void OnTabClosing(content::WebContents* contents);

  // Notifies the AppService InstanceRegistry that the browser is closed. The
  // instance will be removed from App Service InstanceRegistry.
  void OnBrowserRemoved();

  // Helper function to update App Service InstanceRegistry.
  void OnInstances(const std::string& app_id,
                   aura::Window* window,
                   const std::string& launch_id,
                   apps::InstanceState state);

  // Notifies that the shelf id is set for browsers.
  void OnSetShelfIDForBrowserWindowContents(content::WebContents* web_contents);

  // Updates the apps state when the browser's visibility is changed.
  void OnWindowVisibilityChanged(const ash::ShelfID& shelf_id,
                                 aura::Window* window,
                                 bool visible);

  // Updates the apps state when the browser is inactivated.
  void SetWindowActivated(const ash::ShelfID& shelf_id,
                          aura::Window* window,
                          bool active);

  // Returns the instance state for `window` based on `visible`.
  apps::InstanceState CalculateVisibilityState(const aura::Window* window,
                                               bool visible) const;

  // Returns the instance state for `window` based on `active`.
  apps::InstanceState CalculateActivatedState(const aura::Window* window,
                                              bool active) const;

  // Return true if the app is opend in a browser.
  bool IsOpenedInBrowser(const std::string& app_id, aura::Window* window) const;

  // Returns an app id for `window` in InstanceRegistry. If there is no `window`
  // in InstanceRegistry, returns an empty string.
  std::string GetAppId(const aura::Window* window) const;

 private:
  // Returns an app id to represent |contents| in InstanceRegistry. If there is
  // no app in |contents|, returns the app id of the Chrome component
  // application.
  std::string GetAppId(content::WebContents* contents) const;

  // Returns a window to represent |contents| in InstanceRegistry. If |contents|
  // is a Web app, returns the native window for it. If there is no app in
  // |contents|, returns the toplevel window.
  aura::Window* GetWindow(content::WebContents* contents);

  // Returns instances in InstanceRegistry for the given `app_id`.
  std::set<const apps::Instance*> GetInstances(const std::string& app_id);

  // Returns the state in InstanceRegistry for the given `window`. If there is
  // no instance for `window` in InstanceRegistry, returns
  // apps::InstanceState::kUnknown.
  apps::InstanceState GetState(const aura::Window* window) const;

  // Adds the tab's `window` to `browser_window_to_tab_windows_`.
  void AddTabWindow(const std::string& app_id, aura::Window* window);
  // Removes the tab's `window` from `browser_window_to_tab_windows_`.
  void RemoveTabWindow(const std::string& app_id, aura::Window* window);
  // Updates the relation for the tab's `window` and browser's window in
  // `browser_window_to_tab_windows_` and `tab_window_to_browser_window_`.
  void UpdateTabWindow(const std::string& app_id, aura::Window* window);

  raw_ptr<AppServiceAppWindowShelfController> controller_ = nullptr;

  raw_ptr<apps::AppServiceProxy> proxy_ = nullptr;

  // Used to get app info for tabs.
  std::unique_ptr<ShelfControllerHelper> shelf_controller_helper_;

  // Maps the ash Chrome browser window to tab windows in the browser. When the
  // browser window is inactive or invisible, tab windows in the browser should
  // be updated accordingly as well.
  //
  // Note: The Lacros browser should go though BrowserAppShelfController, not
  // via this AppServiceInstanceRegistryHelper.
  std::map<aura::Window*, std::set<raw_ptr<aura::Window, SetExperimental>>>
      browser_window_to_tab_windows_;

  // Maps the tab window to the ash Chrome browser window in the browser.
  //
  // Note: The Lacros browser should go though BrowserAppShelfController, not
  // via this AppServiceInstanceRegistryHelper.
  std::map<aura::Window*, raw_ptr<aura::Window, CtnExperimental>>
      tab_window_to_browser_window_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_
