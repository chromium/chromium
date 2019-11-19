// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"

#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "chrome/browser/chromeos/arc/pip/arc_pip_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"

ArcAppWindowLauncherItemController::ArcAppWindowLauncherItemController(
    const ash::ShelfID shelf_id)
    : AppWindowLauncherItemController(shelf_id) {}

ArcAppWindowLauncherItemController::~ArcAppWindowLauncherItemController() =
    default;

void ArcAppWindowLauncherItemController::AddTaskId(int task_id) {
  task_ids_.insert(task_id);
}

void ArcAppWindowLauncherItemController::RemoveTaskId(int task_id) {
  task_ids_.erase(task_id);
}

bool ArcAppWindowLauncherItemController::HasAnyTasks() const {
  return !task_ids_.empty();
}

void ArcAppWindowLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  if (window_count()) {
    // Tapping the shelf icon of an app that's showing PIP means expanding PIP.
    // Even if the app contains multiple windows, we just expand PIP without
    // showing the menu on the shelf icon.
    for (ui::BaseWindow* window : windows()) {
      aura::Window* native_window = window->GetNativeWindow();
      if (native_window->GetProperty(ash::kWindowStateTypeKey) ==
          ash::WindowStateType::kPip) {
        Profile* profile = ChromeLauncherController::instance()->profile();
        arc::ArcPipBridge* pip_bridge =
            arc::ArcPipBridge::GetForBrowserContext(profile);
        // ClosePip() actually expands PIP.
        pip_bridge->ClosePip();
        std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
        return;
      }
    }
    AppWindowLauncherItemController::ItemSelected(std::move(event), display_id,
                                                  source, std::move(callback));
    return;
  }

  if (task_ids_.empty()) {
    NOTREACHED();
    std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
    return;
  }
  arc::SetTaskActive(*task_ids_.begin());
  std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
}
