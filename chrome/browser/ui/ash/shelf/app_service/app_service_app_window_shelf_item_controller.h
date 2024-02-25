// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"

class AppServiceAppWindowShelfController;

// Shelf item delegate for extension app windows.
class AppServiceAppWindowShelfItemController
    : public AppWindowShelfItemController {
 public:
  explicit AppServiceAppWindowShelfItemController(
      const ash::ShelfID& shelf_id,
      AppServiceAppWindowShelfController* controller);

  ~AppServiceAppWindowShelfItemController() override;

  AppServiceAppWindowShelfItemController(
      const AppServiceAppWindowShelfItemController&) = delete;
  AppServiceAppWindowShelfItemController& operator=(
      const AppServiceAppWindowShelfItemController&) = delete;

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

  // ARC ghost windows introduce session ids to identify the ghost window. So
  // below functions are added to handle session ids for ghost windows.
  void AddSessionId(int session_id);
  void RemoveSessionId(int session_id);
  bool HasAnySessions() const;

 private:
  bool IsChromeApp();

  raw_ptr<AppServiceAppWindowShelfController, DanglingUntriaged> controller_ =
      nullptr;

  std::set<int> task_ids_;
  std::set<int> session_ids_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_SHELF_ITEM_CONTROLLER_H_
