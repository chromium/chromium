// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_test_util.h"

#include <memory>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace {

// A callback that records the action taken when a shelf item is selected.
void ItemSelectedCallback(ash::ShelfAction* action_taken,
                          base::RunLoop* run_loop,
                          ash::ShelfAction action,
                          ash::ShelfItemDelegate::AppMenuItems items) {
  *action_taken = action;
  run_loop->Quit();
}

}  // namespace

ash::ShelfAction SelectShelfItem(const ash::ShelfID& id,
                                 ui::EventType event_type,
                                 int64_t display_id) {
  std::unique_ptr<ui::Event> event;
  if (event_type == ui::ET_MOUSE_PRESSED) {
    event =
        std::make_unique<ui::MouseEvent>(event_type, gfx::Point(), gfx::Point(),
                                         ui::EventTimeForNow(), ui::EF_NONE, 0);
  } else if (event_type == ui::ET_KEY_RELEASED) {
    event = std::make_unique<ui::KeyEvent>(event_type, ui::VKEY_UNKNOWN,
                                           ui::EF_NONE);
  }

  base::RunLoop run_loop;
  ash::ShelfAction action = ash::SHELF_ACTION_NONE;
  ash::ShelfModel* model = ChromeLauncherController::instance()->shelf_model();
  ash::ShelfItemDelegate* delegate = model->GetShelfItemDelegate(id);
  delegate->ItemSelected(
      std::move(event), display_id, ash::LAUNCH_FROM_UNKNOWN,
      base::BindOnce(&ItemSelectedCallback, &action, &run_loop));
  run_loop.Run();
  return action;
}
