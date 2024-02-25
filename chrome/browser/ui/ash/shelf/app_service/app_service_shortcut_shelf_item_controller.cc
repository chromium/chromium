// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_shortcut_shelf_item_controller.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "ui/events/event.h"

AppServiceShortcutShelfItemController::AppServiceShortcutShelfItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {}

AppServiceShortcutShelfItemController::
    ~AppServiceShortcutShelfItemController() = default;

void AppServiceShortcutShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
  ChromeShelfController::instance()->LaunchApp(ash::ShelfID(shelf_id()), source,
                                               ui::EF_NONE, display_id);
  return;
}

void AppServiceShortcutShelfItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppServiceShortcutShelfItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {}

void AppServiceShortcutShelfItemController::Close() {}
