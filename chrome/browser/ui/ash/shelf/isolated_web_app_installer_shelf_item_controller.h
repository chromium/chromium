// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_SHELF_ITEM_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
struct ShelfID;
}  // namespace ash

namespace aura {
class Window;
}  // namespace aura

class IsolatedWebAppInstallerContextMenu;

// Different installer instances will have different ShelfItems, which will each
// has its own IsolatedWebAppInstallerShelfItemController. This class tracks
// the window it starts with, and prohibits tracking of additional windows.
class IsolatedWebAppInstallerShelfItemController
    : public ash::ShelfItemDelegate,
      aura::WindowObserver {
 public:
  explicit IsolatedWebAppInstallerShelfItemController(
      const ash::ShelfID& shelf_id);

  ~IsolatedWebAppInstallerShelfItemController() override;

  static gfx::ImageSkia GetDefaultInstallerShelfIcon();

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;

  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

  // This method is called  to hand off a window to the controller.
  //
  // TODO(https://crbug.com/378932949): Confirm whether this method is still
  // useful after the lacros removal.
  void AddWindow(aura::Window* window);

  // Shelf item must have a non-empty title for accessibility.
  std::u16string GetTitle();

  // aura::WindowObserver
  void OnWindowDestroying(aura::Window* window) override;

 private:
  void UpdateShelfItem();

  // This controller does not support multiple windows.
  raw_ptr<aura::Window> window_ = nullptr;

  std::unique_ptr<IsolatedWebAppInstallerContextMenu> context_menu_;

  base::WeakPtrFactory<IsolatedWebAppInstallerShelfItemController>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_SHELF_ITEM_CONTROLLER_H_
