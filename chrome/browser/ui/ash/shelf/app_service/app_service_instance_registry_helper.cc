// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_instance_registry_helper.h"

#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_constants/constants.h"
#include "components/exo/shell_surface_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
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
    auto* old_window = old_contents->GetNativeView();

    // Get the app_id from the existed instance first. If there is no record for
    // the window, get the app_id from contents. Because when Chrome app open
    // method is changed from windows to tabs, the app_id could be changed based
    // on the URL, e.g. google photos, which might cause instance app_id
    // inconsistent DCHECK error.
    std::string app_id = GetAppId(old_window);
    if (app_id.empty())
      app_id = shelf_controller_helper_->GetAppID(old_contents);

    // If app_id is empty, we should not set it as inactive because this is
    // Chrome's tab.
    if (!app_id.empty()) {
      apps::InstanceState state = GetState(old_window);
      // If the app has been inactive, we don't need to update the instance.
      if ((state & apps::InstanceState::kActive) !=
          apps::InstanceState::kUnknown) {
        state = static_cast<apps::InstanceState>(state &
                                                 ~apps::InstanceState::kActive);
        OnInstances(app_id, old_window, std::string(), state);
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
  auto* window = GetWindow(contents);

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

  OnInstances(app_id, window, std::string(), state);
}

void AppServiceInstanceRegistryHelper::OnTabClosing(
    content::WebContents* contents) {
  auto* window = GetWindow(contents);

  // When the tab is closed, if the window does not exists in the AppService
  // InstanceRegistry, we don't need to update the status.
  const std::string app_id = GetAppId(window);
  if (app_id.empty())
    return;

  RemoveTabWindow(app_id, window);
  OnInstances(app_id, window, std::string(), apps::InstanceState::kDestroyed);
}

void AppServiceInstanceRegistryHelper::OnBrowserRemoved() {
  auto instances = GetInstances(app_constants::kChromeAppId);
  for (const auto* instance : instances) {
    if (!chrome::FindBrowserWithWindow(instance->Window())) {
      // The tabs in the browser should be closed, and tab windows have been
      // removed from |browser_window_to_tab_windows_|.
      DCHECK(
          !base::Contains(browser_window_to_tab_windows_, instance->Window()));

      // The browser is removed if the window can't be found, so update the
      // Chrome window instance as destroyed.
      OnInstances(app_constants::kChromeAppId, instance->Window(),
                  std::string(), apps::InstanceState::kDestroyed);
    }
  }
}

void AppServiceInstanceRegistryHelper::OnInstances(const std::string& app_id,
                                                   aura::Window* window,
                                                   const std::string& launch_id,
                                                   apps::InstanceState state) {
  if (app_id.empty() || !window)
    return;

  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all proxies. If the instance is found from a
  // proxy, still save to that proxy, otherwise, save to the current active user
  // profile's proxy.
  apps::AppServiceProxy* proxy = proxy_;
  for (Profile* profile : controller_->GetProfileList()) {
    auto* proxy_for_profile =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    if (proxy_for_profile->InstanceRegistry().Exists(window)) {
      proxy = proxy_for_profile;
      break;
    }
  }

  apps::InstanceParams params(app_id, window);
  params.launch_id = launch_id;
  params.state = std::make_pair(state, base::Time::Now());
  proxy->InstanceRegistry().CreateOrUpdateInstance(std::move(params));
}

void AppServiceInstanceRegistryHelper::OnSetShelfIDForBrowserWindowContents(
    content::WebContents* contents) {
  // Do not try to update window status on shutdown, because during the shutdown
  // phase, we can't guaranteen the window destroy sequence, and it might cause
  // crash.
  if (browser_shutdown::HasShutdownStarted())
    return;

  auto* window = GetWindow(contents);
  if (!window || !window->GetToplevelWindow())
    return;

  // If the app id is changed, call OnTabInserted to remove the old app id in
  // AppService InstanceRegistry, and insert the new app id.
  std::string app_id = GetAppId(contents);
  const std::string old_app_id = GetAppId(window);
  if (app_id != old_app_id)
    OnTabInserted(contents);

  // When system startup, session restore creates windows before
  // ChromeShelfController is created, so windows restored can’t get the
  // visible and activated status from OnWindowVisibilityChanged and
  // OnWindowActivated. Also web apps are ready at the very late phase which
  // delays the shelf id setting for windows. So check the top window's visible
  // and activated status when we have the shelf id.
  window = window->GetToplevelWindow();
  const std::string top_app_id = GetAppId(window);
  if (!top_app_id.empty()) {
    app_id = top_app_id;
  } else if (window->GetProperty(chromeos::kAppTypeKey) ==
             chromeos::AppType::BROWSER) {
    // For a normal browser window, set the app id as the browser app id.
    app_id = app_constants::kChromeAppId;
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
  if (shelf_id.app_id != app_constants::kChromeAppId) {
    // For Web apps opened in an app window, we need to find the top level
    // window to compare with the parameter |window|, because we save the tab
    // window in AppService InstanceRegistry for Web apps, and we should set the
    // state for the tab window to keep one instance for the Web app.
    auto instances = GetInstances(shelf_id.app_id);
    for (const auto* instance : instances) {
      auto tab_it = tab_window_to_browser_window_.find(instance->Window());
      if (tab_it == tab_window_to_browser_window_.end() ||
          tab_it->second != window) {
        continue;
      }

      // When the user drags a tab to a new browser, or to an other browser, the
      // top window could be changed, so update the relation for the tap window
      // and the browser window.
      UpdateTabWindow(shelf_id.app_id, instance->Window());

      apps::InstanceState state =
          CalculateVisibilityState(instance->Window(), visible);
      OnInstances(shelf_id.app_id, instance->Window(), shelf_id.launch_id,
                  state);
      return;
    }
    return;
  }

  OnInstances(app_constants::kChromeAppId, window, std::string(),
              CalculateVisibilityState(window, visible));

  if (!base::Contains(browser_window_to_tab_windows_, window))
    return;

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (aura::Window* it : browser_window_to_tab_windows_[window]) {
    const std::string app_id = GetAppId(it);
    if (app_id.empty())
      continue;
    OnInstances(app_id, it, std::string(),
                CalculateVisibilityState(it, visible));
  }
}

void AppServiceInstanceRegistryHelper::SetWindowActivated(
    const ash::ShelfID& shelf_id,
    aura::Window* window,
    bool active) {
  if (shelf_id.app_id != app_constants::kChromeAppId) {
    // For Web apps opened in an app window, we need to find the top level
    // window to compare with |window|, because we save the tab
    // window in AppService InstanceRegistry for Web apps, and we should set the
    // state for the tab window to keep one instance for the Web app.
    auto instances = GetInstances(shelf_id.app_id);
    for (const auto* instance : instances) {
      if (instance->Window()->GetToplevelWindow() != window) {
        continue;
      }

      // When the user drags a tab to a new browser, or to an other browser, the
      // top window could be changed, so the relation for the tab window and the
      // browser window.
      UpdateTabWindow(shelf_id.app_id, instance->Window());

      apps::InstanceState state =
          CalculateActivatedState(instance->Window(), active);
      OnInstances(shelf_id.app_id, instance->Window(), shelf_id.launch_id,
                  state);
      return;
    }
    return;
  }

  OnInstances(app_constants::kChromeAppId, window, std::string(),
              CalculateActivatedState(window, active));

  if (!base::Contains(browser_window_to_tab_windows_, window))
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

    constexpr auto kState = static_cast<apps::InstanceState>(
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

    OnInstances(app_id, contents_window, std::string(), kState);
    return;
  }

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (aura::Window* it : browser_window_to_tab_windows_[window]) {
    const std::string app_id = GetAppId(it);
    if (app_id.empty())
      continue;
    OnInstances(app_id, it, std::string(), CalculateActivatedState(it, active));
  }
}

apps::InstanceState AppServiceInstanceRegistryHelper::CalculateVisibilityState(
    const aura::Window* window,
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
    const aura::Window* window,
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
  // Windows created by exo with app/startup ids are not browser windows.
  if (exo::GetShellApplicationId(window) || exo::GetShellStartupId(window))
    return false;

  for (Profile* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    auto app_type = proxy->AppRegistryCache().GetAppType(app_id);
    if (app_type == apps::AppType::kUnknown)
      continue;

    // Skip extensions because the browser controller is responsible for
    // extension windows.
    if (app_type == apps::AppType::kExtension)
      return true;

    if (app_type != apps::AppType::kChromeApp &&
        app_type != apps::AppType::kSystemWeb &&
        app_type != apps::AppType::kWeb) {
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
  for (Profile* profile : controller_->GetProfileList()) {
    content::BrowserContext* browser_context = nullptr;
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    bool found = false;
    proxy->InstanceRegistry().ForInstancesWithWindow(
        window, [&browser_context, &found](const apps::InstanceUpdate& update) {
          browser_context = update.BrowserContext();
          found = true;
        });
    if (found) {
      return !browser_context;
    }
  }
  return true;
}

std::string AppServiceInstanceRegistryHelper::GetAppId(
    const aura::Window* window) const {
  for (Profile* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    std::string app_id = proxy->InstanceRegistry().GetShelfId(window).app_id;
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
  return app_constants::kChromeAppId;
}

aura::Window* AppServiceInstanceRegistryHelper::GetWindow(
    content::WebContents* contents) {
  std::string app_id = shelf_controller_helper_->GetAppID(contents);
  aura::Window* window = contents->GetNativeView();

  // If |app_id| is empty, it is a browser tab. Returns the toplevel window in
  // this case.
  if (app_id.empty())
    window = window->GetToplevelWindow();
  return window;
}

std::set<const apps::Instance*> AppServiceInstanceRegistryHelper::GetInstances(
    const std::string& app_id) {
  std::set<const apps::Instance*> instances;
  for (Profile* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    instances = base::STLSetUnion<std::set<const apps::Instance*>>(
        instances, proxy->InstanceRegistry().GetInstances(app_id));
  }
  return instances;
}

apps::InstanceState AppServiceInstanceRegistryHelper::GetState(
    const aura::Window* window) const {
  for (Profile* profile : controller_->GetProfileList()) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    auto state = proxy->InstanceRegistry().GetState(window);
    if (state != apps::InstanceState::kUnknown)
      return state;
  }
  return apps::InstanceState::kUnknown;
}

void AppServiceInstanceRegistryHelper::AddTabWindow(const std::string& app_id,
                                                    aura::Window* window) {
  if (app_id == app_constants::kChromeAppId)
    return;

  aura::Window* top_level_window = window->GetToplevelWindow();
  browser_window_to_tab_windows_[top_level_window].insert(window);
  tab_window_to_browser_window_[window] = top_level_window;
}

void AppServiceInstanceRegistryHelper::RemoveTabWindow(
    const std::string& app_id,
    aura::Window* window) {
  if (app_id == app_constants::kChromeAppId)
    return;

  auto it = tab_window_to_browser_window_.find(window);
  if (it == tab_window_to_browser_window_.end())
    return;

  aura::Window* top_level_window = it->second;

  auto browser_it = browser_window_to_tab_windows_.find(top_level_window);
  DCHECK(browser_it != browser_window_to_tab_windows_.end());
  browser_it->second.erase(window);
  if (browser_it->second.empty())
    browser_window_to_tab_windows_.erase(browser_it);
  tab_window_to_browser_window_.erase(it);
}

void AppServiceInstanceRegistryHelper::UpdateTabWindow(
    const std::string& app_id,
    aura::Window* window) {
  RemoveTabWindow(app_id, window);
  AddTabWindow(app_id, window);
}
