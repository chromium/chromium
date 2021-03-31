// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_instance_registry_helper.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_delegate.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace apps {
class InstanceUpdate;
}  // namespace apps

class AppServiceAppWindowCrostiniTracker;
class AppServiceAppWindowArcTracker;
class AppWindowBase;
class ChromeLauncherController;
class Profile;

// AppServiceAppWindowLauncherController observes the AppService
// InstanceRegistry and the aura window manager. It manages app shelf items,
// handles adding and removing launcher items from ChromeLauncherController and
// puts running apps on the Chrome OS shelf.
class AppServiceAppWindowLauncherController
    : public AppWindowLauncherController,
      public aura::EnvObserver,
      public aura::WindowObserver,
      public apps::InstanceRegistry::Observer,
      public ArcAppWindowDelegate {
 public:
  using ProfileList = std::vector<Profile*>;

  explicit AppServiceAppWindowLauncherController(
      ChromeLauncherController* owner);
  ~AppServiceAppWindowLauncherController() override;

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;
  void ActiveUserChanged(const std::string& user_email) override;
  void AdditionalUserAddedToSession(Profile* profile) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
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

  // ArcAppWindowDelegate:
  int GetActiveTaskId() const override;

  // Removes an AppWindowBase from its AppWindowLauncherItemController by
  // |window|.
  void UnregisterWindow(aura::Window* window);

  // Creates an AppWindowBase, adds it to |aura_window_to_app_window_|,
  // and updates its AppWindowLauncherItemController by |window| and |shelf_id|.
  // This function is used by AppServiceAppWindowArcTracker when the task id is
  // created after the window created, to make sure the AppWindowBase and
  // the shelf item are created.
  void AddWindowToShelf(aura::Window* window, const ash::ShelfID& shelf_id);

  AppServiceInstanceRegistryHelper* app_service_instance_helper() {
    return app_service_instance_helper_.get();
  }

  AppServiceAppWindowCrostiniTracker* app_service_crostini_tracker() {
    return crostini_tracker_.get();
  }

  AppWindowBase* GetAppWindow(aura::Window* window);

  void ObserveWindow(aura::Window* window);
  bool IsObservingWindow(aura::Window* window);

  std::vector<aura::Window*> GetArcWindows();

  ProfileList& GetProfileList() { return profile_list_; }

 private:
  using AuraWindowToAppWindow =
      std::map<aura::Window*, std::unique_ptr<AppWindowBase>>;
  using WindowList = std::vector<aura::Window*>;

  void SetWindowActivated(aura::Window* window, bool active);

  // Creates an AppWindowBase and updates its
  // AppWindowLauncherItemController by |window| and |shelf_id|.
  void RegisterWindow(aura::Window* window, const ash::ShelfID& shelf_id);

  // Removes an AppWindowBase from its AppWindowLauncherItemController.
  void UnregisterAppWindow(AppWindowBase* app_window);

  void AddAppWindowToShelf(AppWindowBase* app_window);
  void RemoveAppWindowFromShelf(AppWindowBase* app_window);

  // AppWindowLauncherController:
  void OnItemDelegateDiscarded(ash::ShelfItemDelegate* delegate) override;

  // Returns the shelf id for |window|. |window| could be teleported from the
  // inactive user to the active user, or during the user switch phase, |window|
  // could belong to one of the profile.
  ash::ShelfID GetShelfId(aura::Window* window) const;

  // Returns the app type for the given |app_id|.
  apps::mojom::AppType GetAppType(const std::string& app_id) const;

  // Register |window| if the owner of the given |window| has a window
  // teleported of the |window|'s application type to the current desktop.
  void UserHasAppOnActiveDesktop(aura::Window* window,
                                 const ash::ShelfID& shelf_id,
                                 content::BrowserContext* browser_context);

  AuraWindowToAppWindow aura_window_to_app_window_;
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_{this};

  apps::AppServiceProxyChromeOs* proxy_ = nullptr;
  std::unique_ptr<AppServiceInstanceRegistryHelper>
      app_service_instance_helper_;
  std::unique_ptr<AppServiceAppWindowArcTracker> arc_tracker_;
  std::unique_ptr<AppServiceAppWindowCrostiniTracker> crostini_tracker_;

  // A list of profiles which we additionally observe.
  ProfileList profile_list_;

  // A list of windows added for users.
  WindowList window_list_;

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_LAUNCHER_CONTROLLER_H_
