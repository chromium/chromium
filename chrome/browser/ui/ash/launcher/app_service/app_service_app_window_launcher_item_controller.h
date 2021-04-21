// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include <set>

#include "chrome/browser/ui/ash/launcher/app_window_shelf_item_controller.h"

class AppServiceAppWindowLauncherController;

// Shelf item delegate for extension app windows.
class AppServiceAppWindowLauncherItemController
    : public AppWindowShelfItemController {
 public:
  explicit AppServiceAppWindowLauncherItemController(
      const ash::ShelfID& shelf_id,
      AppServiceAppWindowLauncherController* controller);

  ~AppServiceAppWindowLauncherItemController() override;

  AppServiceAppWindowLauncherItemController(
      const AppServiceAppWindowLauncherItemController&) = delete;
  AppServiceAppWindowLauncherItemController& operator=(
      const AppServiceAppWindowLauncherItemController&) = delete;

  // AppWindowShelfItemController:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;
  AppMenuItems GetAppMenuItems(
      int event_flags,
      const ItemFilterPredicate& filter_predicate) override;

  // aura::WindowObserver overrides:
  void OnWindowTitleChanged(aura::Window* window) override;

  void AddTaskId(int task_id);
  void RemoveTaskId(int task_id);
  bool HasAnyTasks() const;

 private:
  bool IsChromeApp();

  AppServiceAppWindowLauncherController* controller_ = nullptr;

  std::set<int> task_ids_;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
