// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_CROSTINI_TRACKER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_CROSTINI_TRACKER_H_

#include "ash/public/cpp/shelf_types.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/shelf/crostini_app_display.h"

class AppServiceAppWindowShelfController;

namespace aura {
class Window;
}

class Profile;

// AppServiceAppWindowCrostiniTracker is used to handle Crostini app window
// special cases, e.g. CrostiniAppDisplay, Crostini shelf app id, etc.
class AppServiceAppWindowCrostiniTracker {
 public:
  explicit AppServiceAppWindowCrostiniTracker(
      AppServiceAppWindowShelfController* app_service_controller);
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

  // Checks the current app id saved in InstanceRegistry for `window`. If the
  // save app id is not `app_id`, remove the instance and add it back with
  // `app_id` to modify the app id saved in InstanceRegistry. The app id in
  // InstanceRegistry can't be modified directly, so we have to remove it first,
  // then add it back again.
  void MaybeModifyInstance(Profile* profile,
                           aura::Window* window,
                           const std::string& app_id) const;

  const raw_ptr<AppServiceAppWindowShelfController> app_service_controller_;

  CrostiniAppDisplay crostini_app_display_;

  // Windows that have been granted the permission to activate via the
  // exo::Permission window property.
  base::flat_set<raw_ptr<aura::Window, CtnExperimental>>
      activation_permissions_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_CROSTINI_TRACKER_H_
