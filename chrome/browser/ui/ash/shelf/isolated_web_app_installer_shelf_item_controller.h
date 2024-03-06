// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_SHELF_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ISOLATED_WEB_APP_INSTALLER_SHELF_ITEM_CONTROLLER_H_

#include <memory>

#include "chrome/browser/ui/ash/shelf/lacros_shelf_item_controller.h"
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
    : public LacrosShelfItemController,
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

  // LacrosShelfItemController overrides:
  void AddWindow(aura::Window* window) override;
  std::u16string GetTitle() override;

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
