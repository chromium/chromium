// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/arc_playstore_shortcut_shelf_item_controller.h"

ChromeShelfItemFactory::ChromeShelfItemFactory() = default;

ChromeShelfItemFactory::~ChromeShelfItemFactory() = default;

bool ChromeShelfItemFactory::CreateShelfItemForAppId(
    const std::string& app_id,
    ash::ShelfItem* item,
    std::unique_ptr<ash::ShelfItemDelegate>* delegate) {
  ash::ShelfID shelf_id = ash::ShelfID(app_id);
  ash::ShelfItem shelf_item;
  shelf_item.id = shelf_id;
  *item = shelf_item;

  if (app_id == arc::kPlayStoreAppId) {
    *delegate = std::make_unique<ArcPlaystoreShortcutShelfItemController>();
    return true;
  }

  *delegate = std::make_unique<AppShortcutShelfItemController>(shelf_id);
  return true;
}
