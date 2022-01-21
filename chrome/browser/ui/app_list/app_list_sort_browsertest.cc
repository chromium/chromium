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

  enum class AnimationTargetStatus {
    // Animation should be completed normally.
    kCompleted,

    // Animation should be aborted.
    kAborted,

    // Animation does not run.
    // TODO(https://crbug.com/1287334): Use only because the reorder animation
    // in tablet mode is not implemented yet. Remove it when the animation in
    // tablet mode is finished.
    kNotRun
  };

  // Reorders the app list items through the specified context menu indicated by
  // `menu_type`. `target_status` is the reorder animation's target status.
  void ReorderByMouseClickAtContextMenu(ash::AppListSortOrder order,
                                        MenuType menu_type,
                                        AnimationTargetStatus target_status) {
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

    if (target_status != AnimationTargetStatus::kNotRun)
      RegisterReorderAnimationDoneCallback(target_status);

    // Click at the sorting option.
    event_generator_->MoveMouseTo(point_on_option);
    event_generator_->ClickLeftButton();

    if (target_status == AnimationTargetStatus::kCompleted)
      WaitForReorderAnimation();
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

  // extensions::ExtensionBrowserTest:
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

  void TearDownOnMainThread() override {
    // Verify that there is no pending reorder animation callbacks.
    EXPECT_FALSE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

    // Verify that the actual reorder animation states are expected.
    EXPECT_EQ(expected_reorder_animation_stats_,
              saved_reorder_animation_stats_);

    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void OnReorderAnimationDone(bool abort) {
    saved_reorder_animation_stats_.push_back(
        abort ? AnimationTargetStatus::kAborted
              : AnimationTargetStatus::kCompleted);

    // Callback can be registered without a running loop.
    if (run_loop_)
      run_loop_->Quit();
  }

  // Adds a callback that runs at the end of the reorder animation.
  void RegisterReorderAnimationDoneCallback(
      AnimationTargetStatus target_status) {
    app_list_test_api_.AddReorderAnimationCallback(
        base::BindRepeating(&AppListSortBrowserTest::OnReorderAnimationDone,
                            weak_factory_.GetWeakPtr()));

    expected_reorder_animation_stats_.push_back(target_status);
  }

  // Waits until the reorder animation completes.
  void WaitForReorderAnimation() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
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
  std::unique_ptr<base::RunLoop> run_loop_;

  // The actual states of reorder animations.
  std::vector<AnimationTargetStatus> saved_reorder_animation_stats_;

  // The expected states of reorder animations.
  std::vector<AnimationTargetStatus> expected_reorder_animation_stats_;

  base::test::ScopedFeatureList feature_list_;

  base::WeakPtrFactory<AppListSortBrowserTest> weak_factory_{this};
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
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
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
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
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
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
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
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
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
                                   MenuType::kAppListFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that starting a new reorder before the old animation completes works
// as expected.
IN_PROC_BROWSER_TEST_F(
    AppListSortBrowserTest,
    ContextMenuOnAppListItemSortItemsInTopLevelWithoutWaiting) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger name alphabetical sorting.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kAborted);

  // Verify that the app order does not change because the animation is ongoing.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger another reorder animation without waiting for the current one and
  // wait until the new animation finishes. The previous animation should be
  // aborted.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);

  // TODO(https://crbug.com/1288880): verify the app order after the color
  // sorting result becomes consistent.
}

// Verify that deleting an item during reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       DeleteItemDuringReorderingAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger name alphabetical sorting.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kAborted);

  // Verify that the app order does not change because the animation is ongoing.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  UninstallExtension(app3_id_);
  base::RunLoop().RunUntilIdle();

  AppListModelUpdater* model_updater =
      test::GetModelUpdater(AppListClientImpl::GetInstance());

  // Verify that `app3_id_` cannot be found from `model_updater_`.
  EXPECT_FALSE(model_updater->FindItem(app3_id_));

  // Note that the temporary sorting state ends when uninstalling an app.
  // Therefore the remaining apps are placed following the alphabetical order.
  EXPECT_TRUE(model_updater->FindItem(app2_id_)->position().GreaterThan(
      model_updater->FindItem(app1_id_)->position()));
}

// Verifies that clicking at the reorder undo toast should revert the temporary
// sorting order in bubble launcher.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, UndoTemporarySortingClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Ensure that the reorder undo toast's bounds update.
  app_list_test_api_.GetTopLevelAppsGridView()
      ->GetWidget()
      ->LayoutRootViewIfNecessary();

  // The toast should be visible.
  EXPECT_TRUE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());

  RegisterReorderAnimationDoneCallback(AnimationTargetStatus::kCompleted);

  // Mouse click at the undo button.
  views::View* reorder_undo_toast_button =
      app_list_test_api_.GetBubbleReorderUndoButton();
  event_generator_->MoveMouseTo(
      reorder_undo_toast_button->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  WaitForReorderAnimation();

  // Verify that the default app order is recovered.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // The toast should be hidden.
  EXPECT_FALSE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());
}

// Verifies that clicking at the reorder undo toast should revert the temporary
// sorting order in tablet mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, UndoTemporarySortingTablet) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kNotRun);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Ensure that the reorder undo toast's bounds update.
  app_list_test_api_.GetTopLevelAppsGridView()
      ->GetWidget()
      ->LayoutRootViewIfNecessary();

  // The toast should be visible.
  EXPECT_TRUE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());

  // Mouse click at the undo button.
  views::View* reorder_undo_toast_button =
      app_list_test_api_.GetFullscreenReorderUndoButton();
  event_generator_->MoveMouseTo(
      reorder_undo_toast_button->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  // Verify that the default app order is recovered.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // The toast should be hidden.
  EXPECT_FALSE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, TransitionToTabletCommitsSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Ensure that the reorder undo toast's bounds update.
  app_list_test_api_.GetTopLevelAppsGridView()
      ->GetWidget()
      ->LayoutRootViewIfNecessary();

  // The toast should be visible.
  EXPECT_TRUE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());

  // Transition to tablet mode - verify that the fullscreen launcher does not
  // have undo toast, and that the order of apps is still sorted.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);
  EXPECT_FALSE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Transition back to clamshell, and verify the bubble launcher undo toast is
  // now hidden.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  EXPECT_FALSE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToClamshellCommitsSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kNotRun);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Ensure that the reorder undo toast's bounds update.
  app_list_test_api_.GetTopLevelAppsGridView()
      ->GetWidget()
      ->LayoutRootViewIfNecessary();

  // The toast should be visible.
  EXPECT_TRUE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());

  // Transition to clamshell mode - verify that the bubble launcher does not
  // have undo toast, and that the order of apps is still sorted.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  EXPECT_FALSE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Transition back to tablet mode, and verify the fullscreen launcher undo
  // toast is now hidden.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  EXPECT_FALSE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
}

// Verify that switching to tablet mode when the app list reorder animation in
// clamshell mode is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToTabletModeDuringReorderAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order does not change because the reorder animation is in progress.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kNotRun);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
}
