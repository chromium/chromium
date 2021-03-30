// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_CROSTINI_TRACKER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_CROSTINI_TRACKER_H_

#include "ash/public/cpp/shelf_types.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_display.h"

class AppServiceAppWindowLauncherController;

namespace aura {
class Window;
}

namespace exo {
class Permission;
}

// AppServiceAppWindowCrostiniTracker is used to handle Crostini app window
// special cases, e.g. CrostiniAppDisplay, Crostini shelf app id, etc.
class AppServiceAppWindowCrostiniTracker {
 public:
  explicit AppServiceAppWindowCrostiniTracker(
      AppServiceAppWindowLauncherController* app_service_controller);
  ~AppServiceAppWindowCrostiniTracker();

  AppServiceAppWindowCrostiniTracker(
      const AppServiceAppWindowCrostiniTracker&) = delete;
  AppServiceAppWindowCrostiniTracker& operator=(
      const AppServiceAppWindowCrostiniTracker&) = delete;

  void OnWindowVisibilityChanged(aura::Window* window,
                                 const std::string& shelf_app_id);
  void OnWindowDestroying(aura::Window* window);

  // A Crostini app with |app_id| is requested to launch on display with
  // |display_id|.
  void OnAppLaunchRequested(const std::string& app_id, int64_t display_id);

  // Close app with |shelf_id| and then restart it on |display_id|.
  void Restart(const ash::ShelfID& shelf_id, int64_t display_id);

  std::string GetShelfAppId(aura::Window* window) const;

 private:
  void RegisterCrostiniWindowForForceClose(aura::Window* window,
                                           const std::string& app_name);

  AppServiceAppWindowLauncherController* const app_service_controller_;

  CrostiniAppDisplay crostini_app_display_;

  // Windows that have been granted the permission to activate via the
  // exo::Permission window property.
  base::flat_set<aura::Window*> activation_permissions_;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_CROSTINI_TRACKER_H_
