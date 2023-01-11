// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MENU_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MENU_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

// Which component requests context menus, the app list or shelf.
enum class MenuType {
  kAppList = 0,
  kShelf = 1,
};

// The types of menu items shown in the app list or shelf.
enum class MenuItemType {
  kCommand,           // Performs an action when selected.
  kRadio,             // Can be selected/checked among a group of choices.
  kSeparator,         // Shows a horizontal line separator.
  kSubmenu,           // Presents a submenu within another menu.
  kPublisherCommand,  // Performs an app publisher shortcut action when
                      // selected.
};

struct COMPONENT_EXPORT(APP_TYPES) MenuItem {
  MenuItem(MenuItemType type, int32_t command_id);
  MenuItem(const MenuItem&) = delete;
  MenuItem& operator=(const MenuItem&) = delete;
  ~MenuItem();

  // The type of the menu item.
  MenuItemType type;

  // The menu item command id. Used to identify the command when the menu item
  // is executed.
  int32_t command_id;

  // The grit id of the menu item label. Used when the menu item's type is
  // kCommand, kRadio, or kSubmenu.
  int32_t string_id;

  // The optional nested submenu item list.
  std::vector<std::unique_ptr<MenuItem>> submenu;

  // The radio group id. All MenuItems with type kRadio will be grouped by this
  // ID value in the menu.
  int32_t radio_group_id;

  // Publisher-specific shortcut id. May be empty if not required.
  std::string shortcut_id;

  // The string label for this menu item. Used when the menu item's type is
  // kPublisherCommand.
  std::string label;

  // The icon for the menu item. May be null if the item doesn't have an icon.
  gfx::ImageSkia image;
};

using MenuItemPtr = std::unique_ptr<MenuItem>;

// MenuItems are used to populate context menus, e.g. in the app list or shelf.
// Note: Some menu item types only support a subset of these item features.
// Please update comments below (MenuItemType -> [fields expected for usage])
// when anything changed to MenuItemType or MenuItem.
//
// kCommand    -> [command_id, string_id].
// kRadio      -> [command_id, string_id, radio_group_id].
// kSeparator  -> [command_id].
// kSubmenu    -> [command_id, string_id, submenu].
// kPublisherCommand -> [command_id, shortcut_id, label, image].
//
struct COMPONENT_EXPORT(APP_TYPES) MenuItems {
  MenuItems();
  MenuItems(const MenuItems&) = delete;
  MenuItems& operator=(const MenuItems&) = delete;
  ~MenuItems();

  MenuItems(MenuItems&&);
  MenuItems& operator=(MenuItems&&);

  std::vector<MenuItemPtr> items;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_MENU_H_
