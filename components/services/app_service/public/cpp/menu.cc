// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/menu.h"

namespace apps {

MenuItem::MenuItem(MenuItemType type, int32_t command_id)
    : type(type), command_id(command_id) {}

MenuItem::~MenuItem() = default;

MenuItems::MenuItems() = default;

MenuItems::~MenuItems() = default;

MenuType ConvertMojomMenuTypeToMenuType(apps::mojom::MenuType mojom_menu_type) {
  switch (mojom_menu_type) {
    case apps::mojom::MenuType::kAppList:
      return MenuType::kAppList;
    case apps::mojom::MenuType::kShelf:
      return MenuType::kShelf;
  }
}

}  // namespace apps
