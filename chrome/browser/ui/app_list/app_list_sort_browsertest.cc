// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"

namespace {

// Creates a RunLoop that waits until the context menu of app list item is
// shown.
void WaitUntilItemMenuShown(ash::AppListItemView* item_view) {
  base::RunLoop run_loop;

  // Set the callback that will quit the RunLoop when context menu is shown.
  item_view->SetContextMenuShownCallbackForTest(run_loop.QuitClosure());
  run_loop.Run();

  // Reset the callback.
  item_view->SetContextMenuShownCallbackForTest(base::RepeatingClosure());
}

}  // namespace

class AppListSortBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  AppListSortBrowserTest() = default;
  AppListSortBrowserTest(const AppListSortBrowserTest&) = delete;
  AppListSortBrowserTest& operator=(const AppListSortBrowserTest&) = delete;
  ~AppListSortBrowserTest() override = default;

 protected:
  enum class MenuType {
    // The menu shown by right clicking at the app list page.
    kAppListPageMenu,

    // The menu shown by right clicking at a non-folder item.
    kAppListNonFolderItemMenu,

    // The menu shown by right clicking at a folder item.
    kAppListFolderItemMenu
  };

  // Shows the specified root menu that contains sorting menu options. Returns
  // the root menu after showing.
  views::MenuItemView* ShowRootMenuAndReturn(MenuType menu_type) {
    views::MenuItemView* root_menu = nullptr;

    ash::AppsGridView* apps_grid_view =
        app_list_test_api_.GetTopLevelAppsGridView();
    EXPECT_GT(apps_grid_view->view_model()->view_size(), 0);

    switch (menu_type) {
      case MenuType::kAppListPageMenu:
        event_generator_->MoveMouseTo(
            apps_grid_view->GetBoundsInScreen().CenterPoint());
        event_generator_->ClickRightButton();
        root_menu =
            apps_grid_view->context_menu_for_test()->root_menu_item_view();
        break;
      case MenuType::kAppListNonFolderItemMenu:
      case MenuType::kAppListFolderItemMenu:
        ash::AppListItemView* item_view = nullptr;
        auto* model = apps_grid_view->view_model();
        const bool is_folder_item =
            (menu_type == MenuType::kAppListFolderItemMenu);
        for (size_t index = 0; index < model->view_size(); ++index) {
          ash::AppListItemView* current_view = model->view_at(index);
          if (current_view->is_folder() == is_folder_item) {
            item_view = current_view;
            break;
          }
        }
        EXPECT_TRUE(item_view);
        event_generator_->MoveMouseTo(
            item_view->GetBoundsInScreen().CenterPoint());
        event_generator_->ClickRightButton();

        if (is_folder_item) {
          root_menu =
              item_view->context_menu_for_folder()->root_menu_item_view();
        } else {
          WaitUntilItemMenuShown(item_view);
          ash::AppListMenuModelAdapter* menu_model_adapter =
              item_view->item_menu_model_adapter();
          root_menu = menu_model_adapter->root_for_testing();
        }
        break;
    }

    EXPECT_TRUE(root_menu->SubmenuIsShowing());
    return root_menu;
  }

  // For the app list menu or folder item menus, gets the menu item indicated by
  // `order`.
  views::MenuItemView* GetReorderOptionForAppListOrFolderItemMenu(
      const views::MenuItemView* root_menu,
      const ash::AppListSortOrder order) {
    views::MenuItemView* reorder_option = nullptr;
    switch (order) {
      case ash::AppListSortOrder::kNameAlphabetical:
        reorder_option = root_menu->GetSubmenu()->GetMenuItemAt(1);
        EXPECT_TRUE(reorder_option->title() == u"Name");
        break;
      case ash::AppListSortOrder::kColor:
        reorder_option = root_menu->GetSubmenu()->GetMenuItemAt(2);
        EXPECT_TRUE(reorder_option->title() == u"Color");
        break;
      case ash::AppListSortOrder::kNameReverseAlphabetical:
      case ash::AppListSortOrder::kCustom:
        NOTREACHED();
        return nullptr;
    }
    return reorder_option;
  }

  // For non-folder item menus, gets the menu item indicated by `order`.
  views::MenuItemView* GetReorderOptionForNonFolderItemMenu(
      const views::MenuItemView* root_menu,
      ash::AppListSortOrder order) {
    // Get the last menu item index where the reorder submenu is.
    views::MenuItemView* reorder_item_view =
        root_menu->GetSubmenu()->GetLastItem();
    DCHECK_EQ(reorder_item_view->title(), u"Reorder by");
    return reorder_item_view;
  }

  // Reorders the app list items through the specified context menu indicated by
  // `menu_type`.
  void ReorderByMouseClickAtContextMenu(ash::AppListSortOrder order,
                                        MenuType menu_type) {
    // Custom order is not a menu option.
    ASSERT_NE(order, ash::AppListSortOrder::kCustom);

    views::MenuItemView* root_menu = ShowRootMenuAndReturn(menu_type);

    // Get the "Name" or "Color" option.
    views::MenuItemView* reorder_option = nullptr;
    switch (menu_type) {
      case MenuType::kAppListPageMenu:
      case MenuType::kAppListFolderItemMenu:
        reorder_option =
            GetReorderOptionForAppListOrFolderItemMenu(root_menu, order);
        break;
      case MenuType::kAppListNonFolderItemMenu: {
        // The `reorder_option` cached here is the submenu of the options.
        views::MenuItemView* reorder_submenu =
            GetReorderOptionForNonFolderItemMenu(root_menu, order);
        event_generator_->MoveMouseTo(
            reorder_submenu->GetBoundsInScreen().CenterPoint());
        event_generator_->ClickLeftButton();
        reorder_option = reorder_submenu->GetSubmenu()->GetMenuItemAt(
            GetMenuIndexOfSortingOrder(order));
        break;
      }
    }

    gfx::Point point_on_option =
        reorder_option->GetBoundsInScreen().CenterPoint();

    // Click at the sorting option.
    event_generator_->MoveMouseTo(point_on_option);
    event_generator_->ClickLeftButton();
  }

  // Returns the index of the specified sorting option.
  size_t GetMenuIndexOfSortingOrder(ash::AppListSortOrder order) {
    switch (order) {
      case ash::AppListSortOrder::kNameAlphabetical:
        return 0;
      case ash::AppListSortOrder::kColor:
        return 1;
      case ash::AppListSortOrder::kNameReverseAlphabetical:
      case ash::AppListSortOrder::kCustom:
        NOTREACHED();
        return 0;
    }
  }

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {ash::features::kProductivityLauncher, ash::features::kLauncherAppSort},
        /*disabled_features=*/{});
    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Since the ProductivityLauncher flag is enabled, the sort buttons will
    // only be shown in tablet mode.
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    // Ensure async callbacks are run.
    base::RunLoop().RunUntilIdle();

    // Shows the app list which is initially behind a window in tablet mode.
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::TOGGLE_APP_LIST_FULLSCREEN, {});

    const int default_app_count = app_list_test_api_.GetTopListItemCount();

    if (base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport)) {
      // Assume that there are three default apps, one being the Lacros browser.
      ASSERT_EQ(3, app_list_test_api_.GetTopListItemCount());
    } else {
      // Assume that there are two default apps.
      ASSERT_EQ(2, app_list_test_api_.GetTopListItemCount());
    }

    app1_id_ = LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
    ASSERT_FALSE(app1_id_.empty());
    app2_id_ = LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
    ASSERT_FALSE(app2_id_.empty());
    app3_id_ = LoadExtension(test_data_dir_.AppendASCII("app3"))->id();
    ASSERT_FALSE(app3_id_.empty());
    EXPECT_EQ(default_app_count + 3, app_list_test_api_.GetTopListItemCount());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());

    AppListModelUpdater* model_updater =
        test::GetModelUpdater(AppListClientImpl::GetInstance());

    // Set the IconColor for each app to be used for color sort testing.
    // When ordered by color, the apps should be in the following order:
    //   {app2 (red icon), app3 (green icon), app1 (blue icon)}
    model_updater->SetIconColor(
        app1_id_,
        ash::IconColor(sync_pb::AppListSpecifics_ColorGroup_COLOR_BLUE, 260));
    model_updater->SetIconColor(
        app2_id_,
        ash::IconColor(sync_pb::AppListSpecifics_ColorGroup_COLOR_RED, 5));
    model_updater->SetIconColor(
        app3_id_,
        ash::IconColor(sync_pb::AppListSpecifics_ColorGroup_COLOR_GREEN, 230));
  }

  // Returns a list of app ids (excluding the default installed apps) following
  // the ordinal increasing order.
  std::vector<std::string> GetAppIdsInOrdinalOrder() {
    AppListModelUpdater* model_updater =
        test::GetModelUpdater(AppListClientImpl::GetInstance());
    std::vector<std::string> ids{app1_id_, app2_id_, app3_id_};
    std::sort(ids.begin(), ids.end(),
              [model_updater](const std::string& id1, const std::string& id2) {
                return model_updater->FindItem(id1)->position().LessThan(
                    model_updater->FindItem(id2)->position());
              });
    return ids;
  }

  ash::AppListTestApi app_list_test_api_;
  std::string app1_id_;
  std::string app2_id_;
  std::string app3_id_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the apps in the top level apps grid can be arranged in the
// alphabetical order or sorted by the apps' icon colors using the context menu
// in apps grid view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, ContextMenuSortItemsInTopLevel) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that the apps in a folder can be arranged in the alphabetical order
// or sorted by the apps' icon colors using the context menu in apps grid view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, ContextMenuSortItemsInFolder) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  // Move apps to one folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_, app3_id_});

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that the apps in the top level apps grid can be arranged in the
// alphabetical order or sorted by the apps' icon colors using the context menu
// in app list item view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       ContextMenuOnAppListItemSortItemsInTopLevel) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that the apps in a folder can be arranged in the alphabetical order
// or sorted by the apps' icon colors using the context menu in app list item
// view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       ContextMenuOnAppListItemSortItemsInFolder) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  // Move apps to one folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_, app3_id_});
  app_list_test_api_.GetTopLevelAppsGridView()
      ->GetWidget()
      ->LayoutRootViewIfNecessary();

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that the apps can be arranged in the alphabetical order or sorted by
// the apps' icon colors using the context menu on folder item view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       ContextMenuOnFolderItemSortItems) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  // Move apps to one folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_, app3_id_});
  app_list_test_api_.GetTopLevelAppsGridView()->Layout();

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that clicking at the reorder undo toast should revert the temporary
// sorting order.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, UndoTemporarySorting) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Ensure that the reorder undo toast's bounds update.
  app_list_test_api_.GetTopLevelAppsGridView()
      ->GetWidget()
      ->LayoutRootViewIfNecessary();

  // The toast should be visible.
  EXPECT_TRUE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());

  // Mouse click at the undo button.
  views::View* reorder_undo_toast_button =
      app_list_test_api_.GetBubbleReorderUndoButton();
  event_generator_->MoveMouseTo(
      reorder_undo_toast_button->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  // Verify that the default app order is recovered.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // The toast should be hidden.
  EXPECT_FALSE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());
}
