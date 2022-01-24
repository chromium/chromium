// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_instance_registry_helper.h"

#include <set>
#include <string>
#include <vector>

#include "ash/constants/app_types.h"
#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/exo/shell_surface_util.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

AppServiceInstanceRegistryHelper::AppServiceInstanceRegistryHelper(
    AppServiceAppWindowShelfController* controller)
    : controller_(controller),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(
          controller->owner()->profile())),
      shelf_controller_helper_(std::make_unique<ShelfControllerHelper>(
          controller->owner()->profile())) {
  DCHECK(controller_);
}

AppServiceInstanceRegistryHelper::~AppServiceInstanceRegistryHelper() = default;

void AppServiceInstanceRegistryHelper::ActiveUserChanged() {
  proxy_ = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
}

void AppServiceInstanceRegistryHelper::AdditionalUserAddedToSession() {
  proxy_ = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
}

void AppServiceInstanceRegistryHelper::OnActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (old_contents) {
    auto instance_key = apps::Instance::InstanceKey::ForWebBasedApp(
        old_contents->GetNativeView());

    // Get the app_id from the existed instance first. If there is no record for
    // the window, get the app_id from contents. Because when Chrome app open
    // method is changed from windows to tabs, the app_id could be changed based
    // on the URL, e.g. google photos, which might cause instance app_id
    // inconsistent DCHECK error.
    std::string app_id = GetAppId(instance_key);
    if (app_id.empty())
      app_id = shelf_controller_helper_->GetAppID(old_contents);

    // If app_id is empty, we should not set it as inactive because this is
    // Chrome's tab.
    if (!app_id.empty()) {
      apps::InstanceState state = GetState(instance_key);
      // If the app has been inactive, we don't need to update the instance.
      if ((state & apps::InstanceState::kActive) !=
          apps::InstanceState::kUnknown) {
        state = static_cast<apps::InstanceState>(state &
                                                 ~apps::InstanceState::kActive);
        OnInstances(GetInstanceKeyForWebContents(old_contents), app_id,
                    std::string(), state);
      }
    }
  }

  if (new_contents) {
    apps::Instance::InstanceKey instance_key =
        GetInstanceKeyForWebContents(new_contents);

    // Get the app_id from the existed instance first. If there is no record for
    // the window, get the app_id from contents. Because when Chrome app open
    // method is changed from windows to tabs, the app_id could be changed based
    // on the URL, e.g. google photos, which might cause instance app_id
    // inconsistent DCHECK error.
    std::string app_id = GetAppId(instance_key);
    if (app_id.empty())
      app_id = GetAppId(new_contents);

    // When the user drags a tab to a new browser, or to an other browser, the
    // top window could be changed, so the relation for the tap window and the
    // browser window.
    UpdateTabInstance(app_id, instance_key);

    // If the app is active, it should be started, running, and visible.
    apps::InstanceState state = static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
    OnInstances(instance_key, app_id, std::string(), state);
  }
}

void AppServiceInstanceRegistryHelper::OnTabReplaced(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  OnTabClosing(old_contents);
  OnTabInserted(new_contents);
}

void AppServiceInstanceRegistryHelper::OnTabInserted(
    content::WebContents* contents) {
  std::string app_id = GetAppId(contents);
  apps::Instance::InstanceKey instance_key =
      GetInstanceKeyForWebContents(contents);

  // When the user drags a tab to a new browser, or to an other browser, it
  // could generate a temp instance for this window with the Chrome application
  // app_id. For this case, this temp instance can be deleted, otherwise, DCHECK
  // error for inconsistent app_id.
  const std::string old_app_id = GetAppId(instance_key);
  if (!old_app_id.empty() && app_id != old_app_id) {
    RemoveTabInstance(old_app_id, instance_key);
    OnInstances(instance_key, old_app_id, std::string(),
                apps::InstanceState::kDestroyed);
  }

  // The tab window could be dragged to a new browser, and the top window could
  // be changed, so clear the old top window first, then add the new top window.
  UpdateTabInstance(app_id, instance_key);
  apps::InstanceState state = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);

  OnInstances(instance_key, app_id, std::string(), state);
}

void AppServiceInstanceRegistryHelper::OnTabClosing(
    content::WebContents* contents) {
  apps::Instance::InstanceKey instance_key =
      GetInstanceKeyForWebContents(contents);

  // When the tab is closed, if the window does not exists in the AppService
  // InstanceRegistry, we don't need to update the status.
  const std::string app_id = GetAppId(instance_key);
  if (app_id.empty())
    return;

  RemoveTabInstance(app_id, instance_key);
  OnInstances(instance_key, app_id, std::string(),
              apps::InstanceState::kDestroyed);
}

void AppServiceInstanceRegistryHelper::OnBrowserRemoved() {
  auto instance_keys = GetInstanceKeys(extension_misc::kChromeAppId);
  for (const apps::Instance::InstanceKey& instance_key : instance_keys) {
    DCHECK(!instance_key.IsForWebBasedApp());
    if (!chrome::FindBrowserWithWindow(instance_key.GetEnclosingAppWindow())) {
      // The tabs in the browser should be closed, and tab windows have been
      // removed from |browser_window_to_tab_window_|.
      DCHECK(!base::Contains(browser_window_to_tab_instances_,
                             instance_key.GetEnclosingAppWindow()));

      // The browser is removed if the window can't be found, so update the
      // Chrome window instance as destroyed.
      OnInstances(instance_key, extension_misc::kChromeAppId, std::string(),
                  apps::InstanceState::kDestroyed);
    }
  }
}

void AppServiceInstanceRegistryHelper::OnInstances(
    const apps::Instance::InstanceKey& instance_key,
    const std::string& app_id,
    const std::string& launch_id,
    apps::InstanceState state) {
  if (app_id.empty() || !instance_key.IsValid())
    return;

  std::unique_ptr<apps::Instance> instance = std::make_unique<apps::Instance>(
      app_id, apps::Instance::InstanceKey(instance_key));
  instance->SetLaunchId(launch_id);
  instance->UpdateState(state, base::Time::Now());

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  deltas.push_back(std::move(instance));

  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all proxies. If the instance is found from a
  // proxy, still save to that proxy, otherwise, save to the current active user
  // profile's proxy.
  apps::AppServiceProxy* proxy = proxy_;
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy_for_profile =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    if (proxy_for_profile->InstanceRegistry().Exists(instance_key)) {
      proxy = proxy_for_profile;
      break;
    }
  }
  proxy->InstanceRegistry().OnInstances(std::move(deltas));
}

void AppServiceInstanceRegistryHelper::OnSetShelfIDForBrowserWindowContents(
    content::WebContents* contents) {
  // Do not try to update window status on shutdown, because during the shutdown
  // phase, we can't guaranteen the window destroy sequence, and it might cause
  // crash.
  if (browser_shutdown::HasShutdownStarted())
    return;

  apps::Instance::InstanceKey instance_key =
      GetInstanceKeyForWebContents(contents);
  if (!instance_key.IsValid() || !instance_key.GetEnclosingAppWindow())
    return;

  // If the app id is changed, call OnTabInserted to remove the old app id in
  // AppService InstanceRegistry, and insert the new app id.
  std::string app_id = GetAppId(contents);
  const std::string old_app_id = GetAppId(instance_key);
  if (app_id != old_app_id)
    OnTabInserted(contents);

  // When system startup, session restore creates windows before
  // ChromeShelfController is created, so windows restored can’t get the
  // visible and activated status from OnWindowVisibilityChanged and
  // OnWindowActivated. Also web apps are ready at the very late phase which
  // delays the shelf id setting for windows. So check the top window's visible
  // and activated status when we have the shelf id.
  aura::Window* window = instance_key.GetEnclosingAppWindow();
  const std::string top_app_id =
      GetAppId(apps::Instance::InstanceKey::ForWindowBasedApp(window));
  if (!top_app_id.empty()) {
    app_id = top_app_id;
  } else if (static_cast<ash::AppType>(window->GetProperty(
                 aura::client::kAppType)) == ash::AppType::BROWSER) {
    // For a normal browser window, set the app id as the browser app id.
    app_id = extension_misc::kChromeAppId;
  }
  OnWindowVisibilityChanged(ash::ShelfID(app_id), window, window->IsVisible());
  auto* client = wm::GetActivationClient(window->GetRootWindow());
  if (client) {
    SetWindowActivated(ash::ShelfID(app_id), window,
                       /*active*/ window == client->GetActiveWindow());
  }
}

void AppServiceInstanceRegistryHelper::OnWindowVisibilityChanged(
    const ash::ShelfID& shelf_id,
    aura::Window* window,
    bool visible) {
  if (shelf_id.app_id != extension_misc::kChromeAppId) {
    // For Web apps opened in an app window, we need to find the top level
    // window to compare with the parameter |window|, because we save the tab
    // window in AppService InstanceRegistry for Web apps, and we should set the
    // state for the tab window to keep one instance for the Web app.
    auto instance_keys = GetInstanceKeys(shelf_id.app_id);
    for (const apps::Instance::InstanceKey& instance_key_it : instance_keys) {
      auto tab_it = tab_instance_to_browser_window_.find(instance_key_it);
      if (tab_it == tab_instance_to_browser_window_.end() ||
          tab_it->second != window) {
        continue;
      }

      // When the user drags a tab to a new browser, or to an other browser, the
      // top window could be changed, so update the relation for the tap window
      // and the browser window.
      UpdateTabInstance(shelf_id.app_id, instance_key_it);

      apps::InstanceState state =
          CalculateVisibilityState(instance_key_it, visible);
      OnInstances(instance_key_it, shelf_id.app_id, shelf_id.launch_id, state);
      return;
    }
    return;
  }

  auto instance_key = apps::Instance::InstanceKey::ForWindowBasedApp(window);
  apps::InstanceState state = CalculateVisibilityState(instance_key, visible);
  OnInstances(instance_key, extension_misc::kChromeAppId, std::string(), state);

  if (!base::Contains(browser_window_to_tab_instances_, window))
    return;

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (const auto& it : browser_window_to_tab_instances_[window]) {
    const std::string app_id = GetAppId(it);
    if (app_id.empty())
      continue;
    apps::InstanceState state = CalculateVisibilityState(it, visible);
    OnInstances(it, app_id, std::string(), state);
  }
}

void AppServiceInstanceRegistryHelper::SetWindowActivated(
    const ash::ShelfID& shelf_id,
    aura::Window* window,
    bool active) {
  if (shelf_id.app_id != extension_misc::kChromeAppId) {
    // For Web apps opened in an app window, we need to find the top level
    // window to compare with |window|, because we save the tab
    // window in AppService InstanceRegistry for Web apps, and we should set the
    // state for the tab window to keep one instance for the Web app.
    auto instance_keys = GetInstanceKeys(shelf_id.app_id);
    for (const apps::Instance::InstanceKey& instance_key_it : instance_keys) {
      if (instance_key_it.GetEnclosingAppWindow()->GetToplevelWindow() !=
          window) {
        continue;
      }

      // When the user drags a tab to a new browser, or to an other browser, the
      // top window could be changed, so the relation for the tab window and the
      // browser window.
      UpdateTabInstance(shelf_id.app_id, instance_key_it);

      apps::InstanceState state =
          CalculateActivatedState(instance_key_it, active);
      OnInstances(instance_key_it, shelf_id.app_id, shelf_id.launch_id, state);
      return;
    }
    return;
  }

  auto instance_key = apps::Instance::InstanceKey::ForWindowBasedApp(window);
  apps::InstanceState state = CalculateActivatedState(instance_key, active);
  OnInstances(instance_key, extension_misc::kChromeAppId, std::string(), state);

  if (!base::Contains(browser_window_to_tab_instances_, window))
    return;

  // For the Chrome browser, when the window is activated, the active tab is set
  // as started, running, visible and active state.
  if (active) {
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (!browser)
      return;

    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!contents)
      return;

    apps::InstanceState state = static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
    apps::Instance::InstanceKey contents_instance_key =
        GetInstanceKeyForWebContents(contents);

    // Get the app_id from the existed instance first. The app_id for PWAs could
    // be changed based on the URL, e.g. google photos, which might cause
    // instance app_id inconsistent DCHECK error.
    std::string app_id = GetAppId(contents_instance_key);
    app_id = app_id.empty() ? GetAppId(contents) : app_id;

    // When the user drags a tab to a new browser, or to an other browser, the
    // top window could be changed, so the relation for the tap window and the
    // browser window.
    UpdateTabInstance(app_id, contents_instance_key);

    OnInstances(contents_instance_key, app_id, std::string(), state);
    return;
  }

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (const auto& it : browser_window_to_tab_instances_[window]) {
    const std::string app_id = GetAppId(it);
    if (app_id.empty())
      continue;
    apps::InstanceState state = CalculateActivatedState(it, active);
    OnInstances(it, app_id, std::string(), state);
  }
}

apps::InstanceState AppServiceInstanceRegistryHelper::CalculateVisibilityState(
    const apps::Instance::InstanceKey& instance_key,
    bool visible) const {
  apps::InstanceState state = GetState(instance_key);
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state = (visible) ? static_cast<apps::InstanceState>(
                          state | apps::InstanceState::kVisible)
                    : static_cast<apps::InstanceState>(
                          state & ~(apps::InstanceState::kVisible));
  return state;
}

apps::InstanceState AppServiceInstanceRegistryHelper::CalculateActivatedState(
    const apps::Instance::InstanceKey& instance_key,
    bool active) const {
  // If the app is active, it should be started, running, and visible.
  if (active) {
    return static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
  }

  apps::InstanceState state = GetState(instance_key);
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state =
      static_cast<apps::InstanceState>(state & ~apps::InstanceState::kActive);
  return state;
}

bool AppServiceInstanceRegistryHelper::IsOpenedInBrowser(
    const std::string& app_id,
    aura::Window* window) const {
  // Crostini Terminal App with the app_id kCrostiniTerminalSystemAppId is a
  // System Web App.
  if (app_id == crostini::kCrostiniTerminalSystemAppId)
    return true;

  // Windows created by exo with app/startup ids are not browser windows.
  if (exo::GetShellApplicationId(window) || exo::GetShellStartupId(window))
    return false;

  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    apps::mojom::AppType app_type =
        proxy->AppRegistryCache().GetAppType(app_id);
    if (app_type == apps::mojom::AppType::kUnknown)
      continue;

    if (app_type != apps::mojom::AppType::kExtension &&
        app_type != apps::mojom::AppType::kSystemWeb &&
        app_type != apps::mojom::AppType::kWeb) {
      return false;
    }
  }

  // For Extension apps, and Web apps, AppServiceAppWindowShelfController
  // should only handle Chrome apps, managed by extensions::AppWindow, which
  // should set |browser_context| in AppService InstanceRegistry. So if
  // |browser_context| is not null, the app is a Chrome app,
  // AppServiceAppWindowShelfController should handle it, otherwise, it is
  // opened in a browser, and AppServiceAppWindowShelfController should skip
  // them.
  //
  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all proxies.
  for (auto* profile : controller_->GetProfileList()) {
    content::BrowserContext* browser_context = nullptr;
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    bool found = false;
    proxy->InstanceRegistry().ForOneInstance(
        apps::Instance::InstanceKey::ForWindowBasedApp(window),
        [&browser_context, &found](const apps::InstanceUpdate& update) {
          browser_context = update.BrowserContext();
          found = true;
        });
    if (!found)
      continue;
    return (browser_context) ? false : true;
  }
  return true;
}

std::string AppServiceInstanceRegistryHelper::GetAppId(
    const apps::Instance::InstanceKey& instance_key) const {
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    std::string app_id =
        proxy->InstanceRegistry().GetShelfId(instance_key).app_id;
    if (!app_id.empty())
      return app_id;
  }
  return std::string();
}

std::string AppServiceInstanceRegistryHelper::GetAppId(
    content::WebContents* contents) const {
  std::string app_id = shelf_controller_helper_->GetAppID(contents);
  if (!app_id.empty())
    return app_id;
  return extension_misc::kChromeAppId;
}

apps::Instance::InstanceKey
AppServiceInstanceRegistryHelper::GetInstanceKeyForWebContents(
    content::WebContents* contents) {
  std::string app_id = shelf_controller_helper_->GetAppID(contents);
  aura::Window* window = contents->GetNativeView();

  // If |app_id| is empty, it is a browser tab. Returns the toplevel window in
  // this case.
  if (app_id.empty()) {
    return apps::Instance::InstanceKey::ForWindowBasedApp(
        window->GetToplevelWindow());
  }
  return apps::Instance::InstanceKey::ForWebBasedApp(window);
}

std::set<apps::Instance::InstanceKey>
AppServiceInstanceRegistryHelper::GetInstanceKeys(const std::string& app_id) {
  std::set<apps::Instance::InstanceKey> instance_keys;
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    auto keys = proxy->InstanceRegistry().GetInstanceKeys(app_id);
    instance_keys = base::STLSetUnion<std::set<apps::Instance::InstanceKey>>(
        instance_keys, keys);
  }
  return instance_keys;
}

apps::InstanceState AppServiceInstanceRegistryHelper::GetState(
    const apps::Instance::InstanceKey& instance_key) const {
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    auto state = proxy->InstanceRegistry().GetState(instance_key);
    if (state != apps::InstanceState::kUnknown)
      return state;
  }
  return apps::InstanceState::kUnknown;
}

void AppServiceInstanceRegistryHelper::AddTabInstance(
    const std::string& app_id,
    const apps::Instance::InstanceKey& instance_key) {
  if (app_id == extension_misc::kChromeAppId)
    return;

  aura::Window* top_level_window =
      instance_key.GetEnclosingAppWindow()->GetToplevelWindow();
  browser_window_to_tab_instances_[top_level_window].insert(instance_key);
  tab_instance_to_browser_window_[instance_key] = top_level_window;
}

void AppServiceInstanceRegistryHelper::RemoveTabInstance(
    const std::string& app_id,
    const apps::Instance::InstanceKey& instance_key) {
  if (app_id == extension_misc::kChromeAppId)
    return;

  auto it = tab_instance_to_browser_window_.find(instance_key);
  if (it == tab_instance_to_browser_window_.end())
    return;

  aura::Window* top_level_window = it->second;

  auto browser_it = browser_window_to_tab_instances_.find(top_level_window);
  DCHECK(browser_it != browser_window_to_tab_instances_.end());
  browser_it->second.erase(instance_key);
  if (browser_it->second.empty())
    browser_window_to_tab_instances_.erase(browser_it);
  tab_instance_to_browser_window_.erase(it);
}

void AppServiceInstanceRegistryHelper::UpdateTabInstance(
    const std::string& app_id,
    const apps::Instance::InstanceKey& instance_key) {
  RemoveTabInstance(app_id, instance_key);
  AddTabInstance(app_id, instance_key);
}
