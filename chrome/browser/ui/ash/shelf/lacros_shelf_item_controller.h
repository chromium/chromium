// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_LACROS_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_LACROS_SHELF_ITEM_CONTROLLER_H_

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_types.h"

namespace aura {
class Window;
}  // namespace aura

// This is a common base shelf item controller for windows that are owned by
// Lacros.
class LacrosShelfItemController : public ash::ShelfItemDelegate {
 public:
  explicit LacrosShelfItemController(const ash::ShelfID& shelf_id)
      : ash::ShelfItemDelegate(shelf_id) {}

  // This method is called by LacrosShelfItemTracker to hand off a window to the
  // controller.
  virtual void AddWindow(aura::Window* window) = 0;

  // Shelf item must have a non-empty title for accessibility.
  virtual std::u16string GetTitle() = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_LACROS_SHELF_ITEM_CONTROLLER_H_
