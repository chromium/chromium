// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/services/app_service/public/cpp/instance_registry.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace apps {
class InstanceUpdate;
}  // namespace apps

class AppWindowBase;
class ChromeLauncherController;

// AppServiceAppWindowLauncherController observes the AppService
// InstanceRegistry and the aura window manager. It manages app shelf items,
// handles adding and removing launcher items from ChromeLauncherController and
// puts running apps on the Chrome OS shelf.
class AppServiceAppWindowLauncherController
    : public AppWindowLauncherController,
      public aura::EnvObserver,
      public aura::WindowObserver,
      public apps::InstanceRegistry::Observer {
 public:
  explicit AppServiceAppWindowLauncherController(
      ChromeLauncherController* owner);
  ~AppServiceAppWindowLauncherController() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* new_active,
      aura::Window* old_active) override;

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* instance_registry) override;

 private:
  using AuraWindowToAppWindow =
      std::map<aura::Window*, std::unique_ptr<AppWindowBase>>;

  // Creates an AppWindow and updates its AppWindowLauncherItemController by
  // |window| and |shelf_id|.
  void RegisterAppWindow(aura::Window* window, const ash::ShelfID& shelf_id);

  // Removes an AppWindow from its AppWindowLauncherItemController.
  void UnregisterAppWindow(AppWindowBase* app_window);

  void AddToShelf(AppWindowBase* app_window);
  void RemoveFromShelf(AppWindowBase* app_window);

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;
  void OnItemDelegateDiscarded(ash::ShelfItemDelegate* delegate) override;

  AuraWindowToAppWindow aura_window_to_app_window_;
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_{this};

  apps::AppServiceProxy* proxy_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_LAUNCHER_CONTROLLER_H_
