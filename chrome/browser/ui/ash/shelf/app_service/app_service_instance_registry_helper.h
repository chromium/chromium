// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_

#include <map>
#include <memory>
#include <set>

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
  void OnInstances(const apps::Instance::InstanceKey& instance_key,
                   const std::string& app_id,
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

  // Returns the instance state for |instance_key| based on |visible|.
  apps::InstanceState CalculateVisibilityState(
      const apps::Instance::InstanceKey& instance_key,
      bool visible) const;

  // Returns the instance state for |instance_key| based on |active|.
  apps::InstanceState CalculateActivatedState(
      const apps::Instance::InstanceKey& instance_key,
      bool active) const;

  // Return true if the app is opend in a browser.
  bool IsOpenedInBrowser(const std::string& app_id, aura::Window* window) const;

  // Returns an app id for |instance_key| in InstanceRegistry. If there is no
  // |instance_key| in InstanceRegistry, returns an empty string.
  std::string GetAppId(const apps::Instance::InstanceKey& instance_key) const;

 private:
  // Returns an app id to represent |contents| in InstanceRegistry. If there is
  // no app in |contents|, returns the app id of the Chrome component
  // application.
  std::string GetAppId(content::WebContents* contents) const;

  // Returns an InstanceKey to represent |contents| in InstanceRegistry. If
  // |contents| is a Web app, returns an InstanceKey representing the
  // WebContents for it. If there is no app in |contents|, returns an
  // InstanceKey for the toplevel window.
  apps::Instance::InstanceKey GetInstanceKeyForWebContents(
      content::WebContents* contents);

  // Returns instance keys in InstanceRegistry for the given |app_id|.
  std::set<apps::Instance::InstanceKey> GetInstanceKeys(
      const std::string& app_id);

  // Returns the state in InstanceRegistry for the given |app_id|. If there is
  // no |instance_key| in InstanceRegistry, returns
  // apps::InstanceState::kUnknown.
  apps::InstanceState GetState(
      const apps::Instance::InstanceKey& instance_key) const;

  // Adds the tab's |instance_key| to |browser_window_to_tab_instance_|.
  void AddTabInstance(const std::string& app_id,
                      const apps::Instance::InstanceKey& instance_key);
  // Removes the tab's |instance_key| from |browser_window_to_tab_instance_|.
  void RemoveTabInstance(const std::string& app_id,
                         const apps::Instance::InstanceKey& instance_key);
  // updates the relation for the tab's |instance_key| and
  // |browser_window_to_tab_instance_|.
  void UpdateTabInstance(const std::string& app_id,
                         const apps::Instance::InstanceKey& instance_key);

  AppServiceAppWindowShelfController* controller_ = nullptr;

  apps::AppServiceProxy* proxy_ = nullptr;

  // Used to get app info for tabs.
  std::unique_ptr<ShelfControllerHelper> shelf_controller_helper_;

  // Maps the browser window to tab instances in the browser. When the browser
  // window is inactive or invisible, tab instances in the browser should be
  // updated accordingly as well.
  std::map<aura::Window*, std::set<apps::Instance::InstanceKey>>
      browser_window_to_tab_instances_;

  // Maps the tab instance to the browser window in the browser.
  std::map<apps::Instance::InstanceKey, aura::Window*>
      tab_instance_to_browser_window_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_
