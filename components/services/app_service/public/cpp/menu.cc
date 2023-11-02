// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/menu.h"

namespace apps {

MenuItem::MenuItem(MenuItemType type, int32_t command_id)
    : type(type), command_id(command_id) {}

MenuItem::~MenuItem() = default;

MenuItems::MenuItems() = default;

MenuItems::~MenuItems() = default;

MenuItems::MenuItems(MenuItems&&) = default;
MenuItems& MenuItems::operator=(MenuItems&&) = default;

MenuType ConvertMojomMenuTypeToMenuType(apps::mojom::MenuType mojom_menu_type) {
  switch (mojom_menu_type) {
    case apps::mojom::MenuType::kAppList:
      return MenuType::kAppList;
    case apps::mojom::MenuType::kShelf:
      return MenuType::kShelf;
  }
}

MenuItemType ConvertMojomMenuItemTypeToMenuItemType(
    apps::mojom::MenuItemType mojom_menu_item_type) {
  switch (mojom_menu_item_type) {
    case apps::mojom::MenuItemType::kCommand:
      return MenuItemType::kCommand;
    case apps::mojom::MenuItemType::kRadio:
      return MenuItemType::kRadio;
    case apps::mojom::MenuItemType::kSeparator:
      return MenuItemType::kSeparator;
    case apps::mojom::MenuItemType::kSubmenu:
      return MenuItemType::kSubmenu;
    case apps::mojom::MenuItemType::kPublisherCommand:
      return MenuItemType::kPublisherCommand;
  }
}

apps::mojom::MenuItemType ConvertMenuItemTypeToMojomMenuItemType(
    MenuItemType menu_item_type) {
  switch (menu_item_type) {
    case MenuItemType::kCommand:
      return apps::mojom::MenuItemType::kCommand;
    case MenuItemType::kRadio:
      return apps::mojom::MenuItemType::kRadio;
    case MenuItemType::kSeparator:
      return apps::mojom::MenuItemType::kSeparator;
    case MenuItemType::kSubmenu:
      return apps::mojom::MenuItemType::kSubmenu;
    case MenuItemType::kPublisherCommand:
      return apps::mojom::MenuItemType::kPublisherCommand;
  }
}

MenuItemPtr ConvertMojomMenuItemToMenuItem(
    const apps::mojom::MenuItemPtr& mojom_menu_item) {
  if (!mojom_menu_item) {
    return nullptr;
  }

  auto menu_item = std::make_unique<MenuItem>(
      ConvertMojomMenuItemTypeToMenuItemType(mojom_menu_item->type),
      mojom_menu_item->command_id);
  menu_item->string_id = mojom_menu_item->string_id;

  for (const auto& mojom_submenu : mojom_menu_item->submenu) {
    menu_item->submenu.push_back(ConvertMojomMenuItemToMenuItem(mojom_submenu));
  }

  menu_item->radio_group_id = mojom_menu_item->radio_group_id;
  menu_item->shortcut_id = mojom_menu_item->shortcut_id;
  menu_item->label = mojom_menu_item->label;
  menu_item->image = mojom_menu_item->image;

  return menu_item;
}

apps::mojom::MenuItemPtr ConvertMenuItemToMojomMenuItem(
    const MenuItemPtr& menu_item) {
  if (!menu_item) {
    return nullptr;
  }

  auto mojom_menu_item = apps::mojom::MenuItem::New();
  mojom_menu_item->type =
      ConvertMenuItemTypeToMojomMenuItemType(menu_item->type);
  mojom_menu_item->command_id = menu_item->command_id;
  mojom_menu_item->string_id = menu_item->string_id;

  for (const auto& submenu : menu_item->submenu) {
    mojom_menu_item->submenu.push_back(ConvertMenuItemToMojomMenuItem(submenu));
  }

  mojom_menu_item->radio_group_id = menu_item->radio_group_id;
  mojom_menu_item->shortcut_id = menu_item->shortcut_id;
  mojom_menu_item->label = menu_item->label;
  mojom_menu_item->image = menu_item->image;

  return mojom_menu_item;
}

MenuItems ConvertMojomMenuItemsToMenuItems(
    const apps::mojom::MenuItemsPtr& mojom_menu_items) {
  MenuItems menu_items;

  if (!mojom_menu_items) {
    return menu_items;
  }

  for (const auto& item : mojom_menu_items->items) {
    menu_items.items.push_back(ConvertMojomMenuItemToMenuItem(item));
  }
  return menu_items;
}

apps::mojom::MenuItemsPtr ConvertMenuItemsToMojomMenuItems(
    const MenuItems& menu_items) {
  apps::mojom::MenuItemsPtr mojom_menu_items = apps::mojom::MenuItems::New();
  for (const auto& item : menu_items.items) {
    mojom_menu_items->items.push_back(ConvertMenuItemToMojomMenuItem(item));
  }
  return mojom_menu_items;
}

base::OnceCallback<void(MenuItems)> MenuItemsToMojomMenuItemsCallback(
    base::OnceCallback<void(apps::mojom::MenuItemsPtr)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(apps::mojom::MenuItemsPtr)> inner_callback,
         MenuItems menu_items) {
        std::move(inner_callback)
            .Run(ConvertMenuItemsToMojomMenuItems(std::move(menu_items)));
      },
      std::move(callback));
}

}  // namespace apps
