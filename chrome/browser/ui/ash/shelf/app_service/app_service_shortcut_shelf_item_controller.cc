// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_shortcut_shelf_item_controller.h"

AppServiceShortcutShelfItemController::AppServiceShortcutShelfItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {}

AppServiceShortcutShelfItemController::
    ~AppServiceShortcutShelfItemController() = default;

void AppServiceShortcutShelfItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {}

void AppServiceShortcutShelfItemController::Close() {}
