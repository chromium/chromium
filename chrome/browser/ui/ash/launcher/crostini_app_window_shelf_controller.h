// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_SHELF_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_SHELF_CONTROLLER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_app_launch_observer.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_display.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"

namespace aura {
class Window;
}

class AppWindowBase;
class ChromeLauncherController;

// A controller to manage Crostini app shelf items. It listens to window events
// and events from the container bridge to put running Crostini apps on the
// Chrome OS shelf.
class CrostiniAppWindowShelfController : public AppWindowLauncherController,
                                         public aura::EnvObserver,
                                         public aura::WindowObserver,
                                         public CrostiniAppLaunchObserver {
 public:
  explicit CrostiniAppWindowShelfController(ChromeLauncherController* owner);
  ~CrostiniAppWindowShelfController() override;

  // AppWindowLauncherController:
  void ActiveUserChanged(const std::string& user_email) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // A Crostini app with |app_id| is requested to launch on display with
  // |display_id|.
  void OnAppLaunchRequested(const std::string& app_id,
                            int64_t display_id) override;

  // Close app with |shelf_id| and then restart it on |display_id|.
  void Restart(const ash::ShelfID& shelf_id, int64_t display_id);

 private:
  using AuraWindowToAppWindow =
      std::map<aura::Window*, std::unique_ptr<AppWindowBase>>;

  void RegisterAppWindow(aura::Window* window, const std::string& shelf_app_id);
  void UnregisterAppWindow(AppWindowBase* app_window);
  void AddToShelf(aura::Window* window, AppWindowBase* app_window);

  // Returns ID of the shelf item that is removed, or a null id.
  ash::ShelfID RemoveFromShelf(aura::Window* window, AppWindowBase* app_window);

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;
  void OnItemDelegateDiscarded(ash::ShelfItemDelegate* delegate) override;

  AuraWindowToAppWindow aura_window_to_app_window_;
  std::set<aura::Window*> observed_windows_;
  CrostiniAppDisplay crostini_app_display_;

  // These two member variables track an app restart request. When
  // app_id_to_restart_ is not empty the controller observes that app and
  // relaunches it when all of its windows are closed.
  std::string app_id_to_restart_;
  int64_t display_id_to_restart_in_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppWindowShelfController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CROSTINI_APP_WINDOW_SHELF_CONTROLLER_H_
