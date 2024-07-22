// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_test_util.h"

#include <memory>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

ash::ShelfAction SelectShelfItem(const ash::ShelfID& id,
                                 ui::EventType event_type,
                                 int64_t display_id,
                                 ash::ShelfLaunchSource source) {
  std::unique_ptr<ui::Event> event;
  if (event_type == ui::EventType::kMousePressed) {
    event =
        std::make_unique<ui::MouseEvent>(event_type, gfx::Point(), gfx::Point(),
                                         ui::EventTimeForNow(), ui::EF_NONE, 0);
  } else if (event_type == ui::EventType::kKeyReleased) {
    event = std::make_unique<ui::KeyEvent>(event_type, ui::VKEY_UNKNOWN,
                                           ui::EF_NONE);
  }

  base::test::TestFuture<ash::ShelfAction, ash::ShelfItemDelegate::AppMenuItems>
      future;
  ChromeShelfController::instance()
      ->shelf_model()
      ->GetShelfItemDelegate(id)
      ->ItemSelected(std::move(event), display_id, source, future.GetCallback(),
                     base::NullCallback());
  auto [action, items] = future.Take();
  return action;
}

void SetRefocusURL(const ash::ShelfID& id, const GURL& url) {
  ash::ShelfModel* model = ChromeShelfController::instance()->shelf_model();
  ash::ShelfItemDelegate* delegate = model->GetShelfItemDelegate(id);
  AppShortcutShelfItemController* item_controller =
      static_cast<AppShortcutShelfItemController*>(delegate);
  item_controller->set_refocus_url(url);
}
