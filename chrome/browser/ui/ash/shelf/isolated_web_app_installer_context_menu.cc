// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/ash/shelf/isolated_web_app_installer_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/callback_forward.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

namespace {

// Create an appropriately sized ImageModel for a menu item icon.
ui::ImageModel GetMenuItemIcon(const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(
      icon,
      /*color_id=*/ui::kColorAshSystemUIMenuIcon, ash::kAppContextMenuIconSize);
}

}  // namespace

IsolatedWebAppInstallerContextMenu::IsolatedWebAppInstallerContextMenu(
    base::OnceClosure close_closure)
    : close_closure_(std::move(close_closure)) {}

IsolatedWebAppInstallerContextMenu::~IsolatedWebAppInstallerContextMenu() =
    default;

void IsolatedWebAppInstallerContextMenu::GetMenuModel(
    GetMenuModelCallback callback) {
  std::unique_ptr<ui::SimpleMenuModel> menu =
      std::make_unique<ui::SimpleMenuModel>(this);

  menu->AddItemWithStringIdAndIcon(ash::MENU_CLOSE,
                                   IDS_SHELF_CONTEXT_MENU_CLOSE,
                                   GetMenuItemIcon(views::kCloseIcon));

  std::move(callback).Run(std::move(menu));
}

void IsolatedWebAppInstallerContextMenu::ExecuteCommand(int command_id,
                                                        int event_flags) {
  // The only command supported for installer windows is "close".
  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::MENU_CLOSE:
      std::move(close_closure_).Run();
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
}
