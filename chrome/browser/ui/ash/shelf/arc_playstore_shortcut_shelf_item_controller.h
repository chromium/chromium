// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_PLAYSTORE_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_PLAYSTORE_SHORTCUT_SHELF_ITEM_CONTROLLER_H_

#include <memory>

#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"

class ArcAppLauncher;

class ArcPlaystoreShortcutShelfItemController
    : public AppShortcutShelfItemController {
 public:
  ArcPlaystoreShortcutShelfItemController();

  ArcPlaystoreShortcutShelfItemController(
      const ArcPlaystoreShortcutShelfItemController&) = delete;
  ArcPlaystoreShortcutShelfItemController& operator=(
      const ArcPlaystoreShortcutShelfItemController&) = delete;

  ~ArcPlaystoreShortcutShelfItemController() override;

  // AppShortcutShelfItemController overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;

 private:
  std::unique_ptr<ArcAppLauncher> playstore_launcher_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_PLAYSTORE_SHORTCUT_SHELF_ITEM_CONTROLLER_H_
