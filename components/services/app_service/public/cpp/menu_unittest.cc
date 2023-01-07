// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/menu.h"

#include "components/services/app_service/public/mojom/types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using MenuTest = testing::Test;

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
TEST_F(MenuTest, MojomConvert) {
  apps::mojom::MenuItemPtr menu_item1 = apps::mojom::MenuItem::New();
  menu_item1->type = apps::mojom::MenuItemType::kCommand;
  menu_item1->command_id = 1;
  menu_item1->string_id = 101;
  menu_item1->radio_group_id = -1;
  menu_item1->shortcut_id = "shortcut_id1";
  menu_item1->label = "label1";

  apps::mojom::MenuItemPtr menu_item2 = apps::mojom::MenuItem::New();
  menu_item2->type = apps::mojom::MenuItemType::kSubmenu;
  menu_item2->command_id = 2;

  apps::mojom::MenuItemPtr menu_item3 = apps::mojom::MenuItem::New();
  menu_item3->type = apps::mojom::MenuItemType::kRadio;
  menu_item3->command_id = 3;
  menu_item3->string_id = 103;
  menu_item3->submenu.push_back(std::move(menu_item2));
  menu_item3->radio_group_id = 0;
  menu_item3->shortcut_id = "shortcut_id3";
  menu_item3->label = "label3";

  auto src_menu_items = mojom::MenuItems::New();
  src_menu_items->items.push_back(std::move(menu_item1));
  src_menu_items->items.push_back(std::move(menu_item3));

  auto dst_menu_items = ConvertMenuItemsToMojomMenuItems(
      ConvertMojomMenuItemsToMenuItems(src_menu_items));
  EXPECT_EQ(2U, dst_menu_items->items.size());

  EXPECT_EQ(apps::mojom::MenuItemType::kCommand,
            dst_menu_items->items[0]->type);
  EXPECT_EQ(1, dst_menu_items->items[0]->command_id);
  EXPECT_EQ(101, dst_menu_items->items[0]->string_id);
  EXPECT_EQ(-1, dst_menu_items->items[0]->radio_group_id);
  EXPECT_EQ("shortcut_id1", dst_menu_items->items[0]->shortcut_id);
  EXPECT_EQ("label1", dst_menu_items->items[0]->label);

  EXPECT_EQ(apps::mojom::MenuItemType::kRadio, dst_menu_items->items[1]->type);
  EXPECT_EQ(3, dst_menu_items->items[1]->command_id);
  EXPECT_EQ(103, dst_menu_items->items[1]->string_id);
  EXPECT_EQ(1U, dst_menu_items->items[1]->submenu.size());
  EXPECT_EQ(apps::mojom::MenuItemType::kSubmenu,
            dst_menu_items->items[1]->submenu[0]->type);
  EXPECT_EQ(2, dst_menu_items->items[1]->submenu[0]->command_id);
  EXPECT_EQ(0, dst_menu_items->items[1]->radio_group_id);
  EXPECT_EQ("shortcut_id3", dst_menu_items->items[1]->shortcut_id);
  EXPECT_EQ("label3", dst_menu_items->items[1]->label);
}

}  // namespace apps
