// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/app_service_instance_registry_helper.h"

#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/exo/shell_surface_util.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

AppServiceInstanceRegistryHelper::AppServiceInstanceRegistryHelper(
    AppServiceAppWindowLauncherController* controller)
    : controller_(controller),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(
          controller->owner()->profile())),
      launcher_controller_helper_(std::make_unique<LauncherControllerHelper>(
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
    auto* window = old_contents->GetNativeView();

    // Get the app_id from the existed instance first. If there is no record for
    // the window, get the app_id from contents. Because when Chrome app open
    // method is changed from windows to tabs, the app_id could be changed based
    // on the URL, e.g. google photos, which might cause instance app_id
    // inconsistent DCHECK error.
    std::string app_id = GetAppId(window);
    if (app_id.empty())
      app_id = launcher_controller_helper_->GetAppID(old_contents);

    // If app_id is empty, we should not set it as inactive because this is
    // Chrome's tab.
    if (!app_id.empty()) {
      apps::InstanceState state = GetState(window);
      // If the app has been inactive, we don't need to update the instance.
      if ((state & apps::InstanceState::kActive) !=
          apps::InstanceState::kUnknown) {
        state = static_cast<apps::InstanceState>(state &
                                                 ~apps::InstanceState::kActive);
        OnInstances(app_id, GetWindow(old_contents), std::string(), state);
      }
    }
  }

  if (new_contents) {
    auto* window = GetWindow(new_contents);

    // Get the app_id from the existed instance first. If there is no record for
    // the window, get the app_id from contents. Because when Chrome app open
    // method is changed from windows to tabs, the app_id could be changed based
    // on the URL, e.g. google photos, which might cause instance app_id
    // inconsistent DCHECK error.
    std::string app_id = GetAppId(window);
    if (app_id.empty())
      app_id = GetAppId(new_contents);

    // When the user drags a tab to a new browser, or to an other browser, the
    // top window could be changed, so the relation for the tap window and the
    // browser window.
    UpdateTabWindow(app_id, window);

    // If the app is active, it should be started, running, and visible.
    apps::InstanceState state = static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
    OnInstances(app_id, window, std::string(), state);
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
  aura::Window* window = GetWindow(contents);

  // When the user drags a tab to a new browser, or to an other browser, it
  // could generate a temp instance for this window with the Chrome application
  // app_id. For this case, this temp instance can be deleted, otherwise, DCHECK
  // error for inconsistent app_id.
  const std::string old_app_id = GetAppId(window);
  if (!old_app_id.empty() && app_id != old_app_id) {
    RemoveTabWindow(old_app_id, window);
    OnInstances(old_app_id, window, std::string(),
                apps::InstanceState::kDestroyed);
  }

  // The tab window could be dragged to a new browser, and the top window could
  // be changed, so clear the old top window first, then add the new top window.
  UpdateTabWindow(app_id, window);
  apps::InstanceState state = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);

  // Observe the tab, because when the system is shutdown or some other cases,
  // the window could be destroyed without calling OnTabClosing. So observe the
  // tab to get the notify when the window is destroyed.
  controller_->ObserveWindow(window);
  OnInstances(app_id, window, std::string(), state);
}

void AppServiceInstanceRegistryHelper::OnTabClosing(
    content::WebContents* contents) {
  aura::Window* window = GetWindow(contents);

  // When the tab is closed, if the window does not exists in the AppService
  // InstanceRegistry, we don't need to update the status.
  const std::string app_id = GetAppId(window);
  if (app_id.empty())
    return;

  RemoveTabWindow(app_id, window);
  OnInstances(app_id, window, std::string(), apps::InstanceState::kDestroyed);
}

void AppServiceInstanceRegistryHelper::OnBrowserRemoved() {
  auto windows = GetWindows(extension_misc::kChromeAppId);
  for (auto* window : windows) {
    if (!chrome::FindBrowserWithWindow(window)) {
      // Remove windows from |browser_window_to_tab_window_| and
      // |tab_window_to_browser_window_|, because OnTabClosing could be not
      // called for tabs in the browser, when the browser is removed.
      if (base::Contains(browser_window_to_tab_window_, window)) {
        for (auto* w : browser_window_to_tab_window_[window]) {
          tab_window_to_browser_window_.erase(w);
          OnInstances(GetAppId(w), w, std::string(),
                      apps::InstanceState::kDestroyed);
        }
        browser_window_to_tab_window_.erase(window);
      }

      // The browser is removed if the window can't be found, so update the
      // Chrome window instance as destroyed.
      OnInstances(extension_misc::kChromeAppId, window, std::string(),
                  apps::InstanceState::kDestroyed);
    }
  }
}

void AppServiceInstanceRegistryHelper::OnInstances(const std::string& app_id,
                                                   aura::Window* window,
                                                   const std::string& launch_id,
                                                   apps::InstanceState state) {
  if (app_id.empty() || !window)
    return;

  // If the window is not observed, this means the window is being destroyed. In
  // this case, don't add the instance because we might keep the record for the
  // destroyed window, which could cause crash.
  if (state != apps::InstanceState::kDestroyed &&
      !controller_->IsObservingWindow(window)) {
    state = apps::InstanceState::kDestroyed;
  }

  std::unique_ptr<apps::Instance> instance =
      std::make_unique<apps::Instance>(app_id, window);
  instance->SetLaunchId(launch_id);
  instance->UpdateState(state, base::Time::Now());

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  deltas.push_back(std::move(instance));

  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all proxies. If the instance is found from a
  // proxy, still save to that proxy, otherwise, save to the current active user
  // profile's proxy.
  apps::AppServiceProxyChromeOs* proxy = proxy_;
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy_for_profile =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    if (proxy_for_profile->InstanceRegistry().Exists(window)) {
      proxy = proxy_for_profile;
      break;
    }
  }
  proxy->InstanceRegistry().OnInstances(std::move(deltas));
}

void AppServiceInstanceRegistryHelper::OnSetShelfIDForBrowserWindowContents(
    content::WebContents* contents) {
  aura::Window* window = GetWindow(contents);
  if (!window || !window->GetToplevelWindow())
    return;

  // If the app id is changed, call OnTabInserted to remove the old app id in
  // AppService InstanceRegistry, and insert the new app id.
  std::string app_id = GetAppId(contents);
  const std::string old_app_id = GetAppId(window);
  if (app_id != old_app_id)
    OnTabInserted(contents);

  // When system startup, session restore creates windows before
  // ChromeLauncherController is created, so windows restored can’t get the
  // visible and activated status from OnWindowVisibilityChanged and
  // OnWindowActivated. Also web apps are ready at the very late phase which
  // delays the shelf id setting for windows. So check the top window's visible
  // and activated status when we have the shelf id.
  window = window->GetToplevelWindow();
  const std::string top_app_id = GetAppId(window);
  if (!top_app_id.empty())
    app_id = top_app_id;
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
    auto windows = GetWindows(shelf_id.app_id);
    for (auto* it : windows) {
      auto tab_it = tab_window_to_browser_window_.find(it);
      if (tab_it == tab_window_to_browser_window_.end() ||
          tab_it->second != window) {
        continue;
      }

      // When the user drags a tab to a new browser, or to an other browser, the
      // top window could be changed, so update the relation for the tap window
      // and the browser window.
      UpdateTabWindow(shelf_id.app_id, it);

      apps::InstanceState state = CalculateVisibilityState(it, visible);
      OnInstances(shelf_id.app_id, it, shelf_id.launch_id, state);
      return;
    }
    return;
  }

  apps::InstanceState state = CalculateVisibilityState(window, visible);
  OnInstances(extension_misc::kChromeAppId, window, std::string(), state);

  if (!base::Contains(browser_window_to_tab_window_, window))
    return;

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (auto* it : browser_window_to_tab_window_[window]) {
    const std::string app_id = GetAppId(it);
    if (app_id.empty())
      continue;
    apps::InstanceState state = CalculateVisibilityState(it, visible);
    OnInstances(app_id, it, std::string(), state);
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
    auto windows = GetWindows(shelf_id.app_id);
    for (auto* it : windows) {
      if (it->GetToplevelWindow() != window)
        continue;

      // When the user drags a tab to a new browser, or to an other browser, the
      // top window could be changed, so the relation for the tap window and the
      // browser window.
      UpdateTabWindow(shelf_id.app_id, it);

      apps::InstanceState state = CalculateActivatedState(it, active);
      OnInstances(shelf_id.app_id, it, shelf_id.launch_id, state);
      return;
    }
    return;
  }

  apps::InstanceState state = CalculateActivatedState(window, active);
  OnInstances(extension_misc::kChromeAppId, window, std::string(), state);

  if (!base::Contains(browser_window_to_tab_window_, window))
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
    auto* contents_window = GetWindow(contents);

    // Get the app_id from the existed instance first. The app_id for PWAs could
    // be changed based on the URL, e.g. google photos, which might cause
    // instance app_id inconsistent DCHECK error.
    std::string app_id = GetAppId(contents_window);
    app_id = app_id.empty() ? GetAppId(contents) : app_id;

    // When the user drags a tab to a new browser, or to an other browser, the
    // top window could be changed, so the relation for the tap window and the
    // browser window.
    UpdateTabWindow(app_id, contents_window);

    OnInstances(app_id, contents_window, std::string(), state);
    return;
  }

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (auto* it : browser_window_to_tab_window_[window]) {
    const std::string app_id = GetAppId(it);
    if (app_id.empty())
      continue;
    apps::InstanceState state = CalculateActivatedState(it, active);
    OnInstances(app_id, it, std::string(), state);
  }
}

apps::InstanceState AppServiceInstanceRegistryHelper::CalculateVisibilityState(
    aura::Window* window,
    bool visible) const {
  apps::InstanceState state = GetState(window);
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state = (visible) ? static_cast<apps::InstanceState>(
                          state | apps::InstanceState::kVisible)
                    : static_cast<apps::InstanceState>(
                          state & ~(apps::InstanceState::kVisible));
  return state;
}

apps::InstanceState AppServiceInstanceRegistryHelper::CalculateActivatedState(
    aura::Window* window,
    bool active) const {
  // If the app is active, it should be started, running, and visible.
  if (active) {
    return static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
  }

  apps::InstanceState state = GetState(window);
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
        app_type != apps::mojom::AppType::kWeb) {
      return false;
    }
  }

  // For Extension apps, and Web apps, AppServiceAppWindowLauncherController
  // should only handle Chrome apps, managed by extensions::AppWindow, which
  // should set |browser_context| in AppService InstanceRegistry. So if
  // |browser_context| is not null, the app is a Chrome app,
  // AppServiceAppWindowLauncherController should handle it, otherwise, it is
  // opened in a browser, and AppServiceAppWindowLauncherController should skip
  // them.
  //
  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all proxies.
  for (auto* profile : controller_->GetProfileList()) {
    content::BrowserContext* browser_context = nullptr;
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    bool found = false;
    proxy->InstanceRegistry().ForOneInstance(
        window, [&browser_context, &found](const apps::InstanceUpdate& update) {
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
    aura::Window* window) const {
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    std::string app_id = proxy->InstanceRegistry().GetShelfId(window).app_id;
    if (!app_id.empty())
      return app_id;
  }
  return std::string();
}

std::string AppServiceInstanceRegistryHelper::GetAppId(
    content::WebContents* contents) const {
  std::string app_id = launcher_controller_helper_->GetAppID(contents);
  if (!app_id.empty())
    return app_id;
  return extension_misc::kChromeAppId;
}

aura::Window* AppServiceInstanceRegistryHelper::GetWindow(
    content::WebContents* contents) {
  std::string app_id = launcher_controller_helper_->GetAppID(contents);
  aura::Window* window = contents->GetNativeView();

  // If |app_id| is empty, it is a browser tab. Returns the toplevel window in
  // this case.
  if (app_id.empty())
    window = window->GetToplevelWindow();
  return window;
}

std::set<aura::Window*> AppServiceInstanceRegistryHelper::GetWindows(
    const std::string& app_id) {
  std::set<aura::Window*> windows;
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    auto w = proxy->InstanceRegistry().GetWindows(app_id);
    windows = base::STLSetUnion<std::set<aura::Window*>>(windows, w);
  }
  return windows;
}

apps::InstanceState AppServiceInstanceRegistryHelper::GetState(
    aura::Window* window) const {
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    auto state = proxy->InstanceRegistry().GetState(window);
    if (state != apps::InstanceState::kUnknown)
      return state;
  }
  return apps::InstanceState::kUnknown;
}

void AppServiceInstanceRegistryHelper::AddTabWindow(const std::string& app_id,
                                                    aura::Window* window) {
  if (app_id == extension_misc::kChromeAppId)
    return;

  aura::Window* top_level_window = window->GetToplevelWindow();
  browser_window_to_tab_window_[top_level_window].insert(window);
  tab_window_to_browser_window_[window] = top_level_window;
}

void AppServiceInstanceRegistryHelper::RemoveTabWindow(
    const std::string& app_id,
    aura::Window* window) {
  if (app_id == extension_misc::kChromeAppId)
    return;

  auto it = tab_window_to_browser_window_.find(window);
  if (it == tab_window_to_browser_window_.end())
    return;

  aura::Window* top_level_window = it->second;

  auto browser_it = browser_window_to_tab_window_.find(top_level_window);
  DCHECK(browser_it != browser_window_to_tab_window_.end());
  browser_it->second.erase(window);
  if (browser_it->second.empty())
    browser_window_to_tab_window_.erase(browser_it);
  tab_window_to_browser_window_.erase(it);
}

void AppServiceInstanceRegistryHelper::UpdateTabWindow(
    const std::string& app_id,
    aura::Window* window) {
  RemoveTabWindow(app_id, window);
  AddTabWindow(app_id, window);
}
