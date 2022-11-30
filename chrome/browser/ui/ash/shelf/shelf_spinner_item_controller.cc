// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"

#include <utility>

#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"

ShelfSpinnerItemController::ShelfSpinnerItemController(
    const std::string& app_id)
    : ash::ShelfItemDelegate(ash::ShelfID(app_id)),
      start_time_(base::Time::Now()) {}

ShelfSpinnerItemController::~ShelfSpinnerItemController() {
  DCHECK(!(host_ && host_->HasApp(app_id())));
}

void ShelfSpinnerItemController::SetHost(
    const base::WeakPtr<ShelfSpinnerController>& host) {
  DCHECK(!host_ || host_.get() == host.get());
  host_ = host;
}

void ShelfSpinnerItemController::ExecuteCommand(bool from_context_menu,
                                                int64_t command_id,
                                                int32_t event_flags,
                                                int64_t display_id) {
  DCHECK(!from_context_menu);
  NOTIMPLEMENTED();
}

void ShelfSpinnerItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeShelfController* controller = ChromeShelfController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void ShelfSpinnerItemController::Close() {
  if (host_) {
    // CloseSpinner can result in |app_id| being deleted, so make a copy of it
    // first.
    const std::string safe_app_id = app_id();
    host_->CloseSpinner(safe_app_id);
  }
}
