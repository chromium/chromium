// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/views/vector_icons.h"

class ExtensionShelfContextMenuBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionShelfContextMenuBrowserTest() = default;
  ~ExtensionShelfContextMenuBrowserTest() override = default;

  const gfx::VectorIcon& GetExpectedLaunchNewIcon(int command_id) {
    if (command_id == ash::USE_LAUNCH_TYPE_REGULAR)
      return views::kNewTabIcon;
    else if (command_id == ash::USE_LAUNCH_TYPE_WINDOW)
      return views::kNewWindowIcon;
    else
      return views::kOpenIcon;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionShelfContextMenuBrowserTest,
                       LaunchNewMenuItemDynamicallyChanges) {
  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));
  PinAppWithIDToShelf(extension->id());

  ash::ShelfModel* shelf_model = ash::ShelfModel::Get();
  ash::ShelfItemDelegate* delegate =
      shelf_model->GetShelfItemDelegate(ash::ShelfID(extension->id()));

  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  delegate->GetContextMenu(display::Display::GetDefaultDisplay().id(),
                           base::BindLambdaForTesting(
                               [&](std::unique_ptr<ui::SimpleMenuModel> model) {
                                 menu_model = std::move(model);
                                 run_loop.Quit();
                               }));
  run_loop.Run();

  auto launch_new_command_index =
      menu_model->GetIndexOfCommandId(ash::LAUNCH_NEW);
  ASSERT_TRUE(launch_new_command_index);

  auto* launch_new_submodel =
      menu_model->GetSubmenuModelAt(launch_new_command_index.value());

  EXPECT_EQ(launch_new_submodel->GetItemCount(), 2u);
  for (size_t launch_new_item_index = 0;
       launch_new_item_index < launch_new_submodel->GetItemCount();
       ++launch_new_item_index) {
    const auto label_from_submenu =
        launch_new_submodel->GetLabelAt(launch_new_item_index);
    launch_new_submodel->ActivatedAt(launch_new_item_index);
    EXPECT_TRUE(launch_new_submodel->IsItemCheckedAt(launch_new_item_index));

    // Parent `LAUNCH_NEW` item label and icon change dynamically after
    // selection.
    EXPECT_EQ(menu_model->GetLabelAt(launch_new_command_index.value()),
              label_from_submenu);
    EXPECT_EQ(menu_model->GetIconAt(launch_new_command_index.value())
                  .GetVectorIcon()
                  .vector_icon(),
              &GetExpectedLaunchNewIcon(
                  launch_new_submodel->GetCommandIdAt(launch_new_item_index)));
  }
}
