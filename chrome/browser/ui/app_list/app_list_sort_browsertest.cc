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
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"

namespace {

gfx::ImageSkia CreateImageSkia(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

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
        for (int index = 0; index < model->view_size(); ++index) {
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
    DCHECK_EQ(reorder_item_view->title(), u"Sort by");
    return reorder_item_view;
  }

  enum class AnimationTargetStatus {
    // Animation should be completed normally.
    kCompleted,

    // Apps grid fade out animation should be aborted.
    kFadeOutAborted,

    // Apps grid fade in animation should be aborted.
    kFadeInAborted,
  };

  // Reorders the app list items through the specified context menu indicated by
  // `menu_type`. `target_status` is the reorder animation's target status.
  void ReorderByMouseClickAtContextMenu(ash::AppListSortOrder order,
                                        MenuType menu_type,
                                        AnimationTargetStatus target_status) {
    // Ensure that the apps grid layout is refreshed before showing the
    // context menu.
    app_list_test_api_.GetTopLevelAppsGridView()
        ->GetWidget()
        ->LayoutRootViewIfNecessary();

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

    RegisterReorderAnimationDoneCallback(target_status);

    // Click at the sorting option.
    event_generator_->MoveMouseTo(point_on_option);
    event_generator_->ClickLeftButton();

    switch (target_status) {
      case AnimationTargetStatus::kCompleted:
        // Wait until the reorder animation is done.
        WaitForReorderAnimation();
        break;
      case AnimationTargetStatus::kFadeOutAborted:
        // The fade out animation starts synchronously so do not wait before
        // animation interruption.
        break;
      case AnimationTargetStatus::kFadeInAborted:
        // Wait until the fade out animation is done. It ensures that the app
        // list is under fade in animation when animation interruption occurs.
        WaitForFadeOutAnimation();
        break;
    }
  }

  void WaitForFadeOutAnimation() {
    ash::AppsGridView* apps_grid_view =
        app_list_test_api_.GetTopLevelAppsGridView();

    if (apps_grid_view->reorder_animation_status_for_test() !=
        ash::AppListReorderAnimationStatus::kFadeOutAnimation) {
      // The apps grid is not under fade out animation so no op.
      return;
    }

    ASSERT_TRUE(!run_loop_ || !run_loop_->running());
    run_loop_ = std::make_unique<base::RunLoop>();
    apps_grid_view->AddFadeOutAnimationDoneClosureForTest(
        run_loop_->QuitClosure());
    run_loop_->Run();
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
    model_updater->FindItem(app1_id_)->SetIcon(
        CreateImageSkia(16, 16, SK_ColorBLUE));
    model_updater->FindItem(app2_id_)->SetIcon(
        CreateImageSkia(16, 16, SK_ColorRED));
    model_updater->FindItem(app3_id_)->SetIcon(
        CreateImageSkia(16, 16, SK_ColorGREEN));
  }

  void TearDownOnMainThread() override {
    // Verify that there is no pending reorder animation callbacks.
    EXPECT_FALSE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

    // Verify that the actual reorder animation states are expected.
    EXPECT_EQ(expected_reorder_animation_stats_,
              saved_reorder_animation_stats_);

    // There should be no active reorder animations.
    EXPECT_FALSE(app_list_test_api_.GetTopLevelAppsGridView()
                     ->IsUnderReorderAnimation());

    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void OnReorderAnimationDone(bool abort,
                              ash::AppListReorderAnimationStatus status) {
    DCHECK(status == ash::AppListReorderAnimationStatus::kFadeOutAnimation ||
           status == ash::AppListReorderAnimationStatus::kFadeInAnimation);

    // Record the animation running result.
    if (abort) {
      saved_reorder_animation_stats_.push_back(
          status == ash::AppListReorderAnimationStatus::kFadeOutAnimation
              ? AnimationTargetStatus::kFadeOutAborted
              : AnimationTargetStatus::kFadeInAborted);
    } else {
      EXPECT_EQ(ash::AppListReorderAnimationStatus::kFadeInAnimation, status);
      saved_reorder_animation_stats_.push_back(
          AnimationTargetStatus::kCompleted);
    }

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

  // Returns a list of app ids following the ordinal increasing order.
  std::vector<std::string> GetAppIdsInOrdinalOrder(
      const std::initializer_list<std::string>& ids) {
    AppListModelUpdater* model_updater =
        test::GetModelUpdater(AppListClientImpl::GetInstance());
    std::vector<std::string> copy_ids(ids);
    std::sort(copy_ids.begin(), copy_ids.end(),
              [model_updater](const std::string& id1, const std::string& id2) {
                return model_updater->FindItem(id1)->position().LessThan(
                    model_updater->FindItem(id2)->position());
              });
    return copy_ids;
  }

  std::vector<std::string> GetAppIdsInOrdinalOrder() {
    return GetAppIdsInOrdinalOrder({app1_id_, app2_id_, app3_id_});
  }

  ash::AppListSortOrder GetPermanentSortingOrder() {
    return static_cast<ash::AppListSortOrder>(
        profile()->GetPrefs()->GetInteger(prefs::kAppListPreferredOrder));
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
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  base::HistogramTester histograms;
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  histograms.ExpectBucketCount(ash::kClamshellReorderActionHistogram,
                               ash::AppListSortOrder::kNameAlphabetical, 1);

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
  histograms.ExpectBucketCount(ash::kClamshellReorderActionHistogram,
                               ash::AppListSortOrder::kColor, 1);
}

// Verifies that clearing pref order by moving an item works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, ClearPrefOrderByItemMove) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_TRUE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());

  std::string app4_id = LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app4_id.empty());
  EXPECT_EQ(GetAppIdsInOrdinalOrder({app1_id_, app2_id_, app3_id_, app4_id}),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_, app4_id}));

  EXPECT_FALSE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical,
            GetPermanentSortingOrder());

  base::HistogramTester histograms;
  app_list_test_api_.ReorderItemInRootByDragAndDrop(/*source_index=*/0,
                                                    /*target_index=*/1);
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetPermanentSortingOrder());
  histograms.ExpectBucketCount(ash::kClamshellPrefOrderClearActionHistogram,
                               ash::AppListOrderUpdateEvent::kItemMoved, 1);
}

// Verifies that the apps in a folder can be arranged in the alphabetical order
// or sorted by the apps' icon colors using the context menu in apps grid view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, ContextMenuSortItemsInFolder) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

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
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

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
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

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
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

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
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger name alphabetical sorting.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

  // Verify that the app order does not change because the animation is ongoing.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger another reorder animation without waiting for the current one and
  // wait until the new animation finishes. The previous animation should be
  // aborted.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);

  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that deleting an item during reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       DeleteItemDuringReorderingAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger name alphabetical sorting.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

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

  base::HistogramTester histograms;
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Wait for one additional frame so that the metric data is collected.
  ui::Compositor* compositor =
      app_list_test_api_.GetTopLevelAppsGridView()->layer()->GetCompositor();
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  histograms.ExpectTotalCount(
      ash::kClamshellReorderAnimationSmoothnessHistogram, 1);

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

  // Wait for the metric data to be collected.
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  // Smoothness of the reorder animation triggered by undo button is recorded.
  histograms.ExpectTotalCount(
      ash::kClamshellReorderAnimationSmoothnessHistogram, 2);

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

  base::HistogramTester histograms;
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  histograms.ExpectBucketCount(ash::kTabletReorderActionHistogram,
                               ash::AppListSortOrder::kNameAlphabetical, 1);

  // The toast should be visible.
  EXPECT_TRUE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());

  // Wait for one additional frame so that the metric data is collected.
  ui::Compositor* compositor =
      app_list_test_api_.GetTopLevelAppsGridView()->layer()->GetCompositor();
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  histograms.ExpectTotalCount(ash::kTabletReorderAnimationSmoothnessHistogram,
                              1);

  // Register a callback for the reorder animation triggered by the undo button.
  RegisterReorderAnimationDoneCallback(AnimationTargetStatus::kCompleted);

  // Mouse click at the undo button.
  views::View* reorder_undo_toast_button =
      app_list_test_api_.GetFullscreenReorderUndoButton();
  event_generator_->MoveMouseTo(
      reorder_undo_toast_button->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  WaitForReorderAnimation();

  // Verify that the default app order is recovered.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // The toast should be hidden.
  EXPECT_FALSE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());

  // Wait for the metric data to be collected.
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  // Smoothness of the reorder animation triggered by undo button is recorded.
  histograms.ExpectTotalCount(ash::kTabletReorderAnimationSmoothnessHistogram,
                              2);
}

// Verify that installing an app under color sort works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, InstallAppUnderColorSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));

  // TODO(https://crbug.com/1293162): verify app 4's position after new item
  // position calculation under color sorting is fixed.
  LoadExtension(test_data_dir_.AppendASCII("app4"));
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
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

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

// Verify that switching to tablet mode when the fade out animation in clamshell
// mode is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToTabletModeDuringFadeOutAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order does not change because the fade out animation is running.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // When switching to the tablet mode, the app list is closed so the
  // temporary sorting order should be committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  base::HistogramTester histograms;
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  histograms.ExpectBucketCount(ash::kTabletReorderActionHistogram,
                               ash::AppListSortOrder::kColor, 1);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that switching to clamshell mode when the fade out animation in tablet
// mode is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToClamshellModeDuringFadeOutAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order does not change because the fade out animation is running.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Before switching to the tablet mode, the app list is closed so the
  // temporary sorting order is committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that switching to tablet mode when the fade in animation in clamshell
// is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToTabletModeDuringFadeInAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kFadeInAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // When switching to the tablet mode, the app list is closed so the
  // temporary sorting order should be committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that switching to clamshell mode when the fade in animation in tablet
// mode is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToClamshellModeDuringFadeInAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kFadeInAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Before switching to the tablet mode, the app list is closed so the
  // temporary sorting order is committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that in clamshell interrupting a fade out animation by starting
// another reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeOutAnimationClamshellMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order does not change because the fade out animation is running.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that in tablet interrupting a fade out animation by starting another
// reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeOutAnimationTabletMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order does not change because the fade out animation is running.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that in clamshell interrupting a fade in animation by starting another
// reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeInAnimationClamshellMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kFadeInAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that in tablet interrupting a fade in animation by starting another
// reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeInAnimationTabletMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kFadeInAborted);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}
