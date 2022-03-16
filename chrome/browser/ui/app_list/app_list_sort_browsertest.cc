// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/strings/safe_sprintf.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"

namespace {

// An app manifest file's data. The app name and icon paths wait for filling.
constexpr char kManifestData[] =
    R"({ )"
    /**/ R"("description": "fake",)"
    /**/ R"("name": "%s",)"
    /**/ R"("manifest_version": 2,)"
    /**/ R"("version": "0",)"
    /**/ R"("icons": %s,)"
    /**/ R"("app": { )"
    /**/ R"("launch": {)"
    /****/ R"("web_url": "https://www.google.com/")"
    /****/ R"(})"
    /**/ R"(})"
    R"(})";

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

  // Finds an folder item view in the provided apps grid.
  ash::AppListItemView* FindFolderItemView(ash::AppsGridView* apps_grid_view) {
    auto* model = apps_grid_view->view_model();
    for (int index = 0; index < model->view_size(); ++index) {
      ash::AppListItemView* current_view = model->view_at(index);
      if (current_view->is_folder())
        return current_view;
    }

    return nullptr;
  }

  // Finds a non-folder item view in the provided apps grid.
  ash::AppListItemView* FindNonFolderItemView(
      ash::AppsGridView* apps_grid_view) {
    auto* model = apps_grid_view->view_model();
    for (int index = 0; index < model->view_size(); ++index) {
      ash::AppListItemView* current_view = model->view_at(index);
      if (!current_view->is_folder())
        return current_view;
    }

    return nullptr;
  }

  // Shows the specified root menu that contains sorting menu options. Returns
  // the root menu after showing.
  views::MenuItemView* ShowRootMenuAndReturn(ash::AppsGridView* apps_grid_view,
                                             MenuType menu_type) {
    views::MenuItemView* root_menu = nullptr;

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
        const bool is_folder_item =
            (menu_type == MenuType::kAppListFolderItemMenu);
        ash::AppListItemView* item_view =
            is_folder_item ? FindFolderItemView(apps_grid_view)
                           : FindNonFolderItemView(apps_grid_view);
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

  void ReorderByMouseClickAtContextMenu(ash::AppListSortOrder order,
                                        MenuType menu_type,
                                        AnimationTargetStatus target_status) {
    ReorderByMouseClickAtContextMenuInAppsGrid(
        app_list_test_api_.GetTopLevelAppsGridView(), order, menu_type,
        target_status);
  }

  // Reorders the app list items through the specified context menu indicated by
  // `menu_type`. `target_status` is the reorder animation's target status.
  void ReorderByMouseClickAtContextMenuInAppsGrid(
      ash::AppsGridView* apps_grid_view,
      ash::AppListSortOrder order,
      MenuType menu_type,
      AnimationTargetStatus target_status) {
    // Ensure that the apps grid layout is refreshed before showing the
    // context menu.
    apps_grid_view->GetWidget()->LayoutRootViewIfNecessary();

    // Custom order is not a menu option.
    ASSERT_NE(order, ash::AppListSortOrder::kCustom);

    views::MenuItemView* root_menu =
        ShowRootMenuAndReturn(apps_grid_view, menu_type);

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

  // Verifies that app list fade out animation is in progress then runs a
  // closure to interrupt reorder.
  void VerifyFadeOutInProgressAndRunInterruptClosure(
      const std::vector<std::string>& expected_ids_in_order,
      base::OnceClosure interrupt_closure) {
    // Verify that there is active reorder animations.
    EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

    // The app order does not change because the fade out animation is running.
    EXPECT_EQ(GetAppIdsInOrdinalOrder(), expected_ids_in_order);

    // Run the closure to interrupt the fade out animation.
    std::move(interrupt_closure).Run();
  }

  // Registers a closure that turns on/off the tablet mode at the start of the
  // fade out animation to interrupt app list reorder. `tablet_mode_enabled` is
  // true when switching to the tablet mode; `tablet_mode_enabled` is false when
  // switching to the clamshell mode. The tablet mode changes synchronously on
  // animation start to avoid the race condition between the fade out animation
  // completion and the task to change the tablet mode state.
  // See https://crbug.com/1302924
  void RegisterModeSwitchClosureOnFadeOutStarted(bool tablet_mode_enabled) {
    auto switch_mode_closure = base::BindOnce(
        [](bool tablet_mode_enabled) {
          ash::ShellTestApi().SetTabletModeEnabledForTest(tablet_mode_enabled);
        },
        tablet_mode_enabled);

    // NOTE: When the fade out animation is interrupted, the app order should
    // not change.
    app_list_test_api_.AddFadeOutAnimationStartClosure(base::BindOnce(
        &AppListSortBrowserTest::VerifyFadeOutInProgressAndRunInterruptClosure,
        weak_factory_.GetWeakPtr(),
        /*expected_ids_in_order=*/GetAppIdsInOrdinalOrder(),
        std::move(switch_mode_closure)));
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
        CreateImageSkia(16, 16, SK_ColorBLUE), /*is_placeholder=*/false);
    model_updater->FindItem(app2_id_)->SetIcon(
        CreateImageSkia(16, 16, SK_ColorRED), /*is_placeholder=*/false);
    model_updater->FindItem(app3_id_)->SetIcon(
        CreateImageSkia(16, 16, SK_ColorGREEN), /*is_placeholder=*/false);
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

      // Verify that the toast container under the clamshell mode does not have
      // a layer after reorder animation completes.
      views::View* toast_container = app_list_test_api_.GetToastContainerView();
      if (toast_container && !ash::Shell::Get()->IsInTabletMode())
        EXPECT_FALSE(toast_container->layer());
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

  // Verifies that all item views are visible.
  void VerifyItemsVisibility() {
    auto* view_model =
        app_list_test_api_.GetTopLevelAppsGridView()->view_model();
    std::vector<std::string> invisible_item_names;
    for (int view_index = 0; view_index < view_model->view_size();
         ++view_index) {
      auto* item_view = view_model->view_at(view_index);
      if (!item_view->GetVisible())
        invisible_item_names.push_back(item_view->item()->name());
    }

    // Invisible items should be none.
    EXPECT_EQ(std::vector<std::string>(), invisible_item_names);
  }

  // Waits until the reorder animation completes.
  void WaitForReorderAnimation() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();

    VerifyItemsVisibility();
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
  EXPECT_TRUE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());

  std::string app4_id = LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app4_id.empty());
  EXPECT_EQ(GetAppIdsInOrdinalOrder({app1_id_, app2_id_, app3_id_, app4_id}),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_, app4_id}));

  EXPECT_FALSE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());
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

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       SortUsingContextMenuOnFolderChildViewClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Create an app list folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  top_level_grid->Layout();

  // Click on the folder to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view = FindFolderItemView(top_level_grid);
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  ReorderByMouseClickAtContextMenuInAppsGrid(
      folder_grid, ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_FALSE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       FolderNotClosedIfTemporarySortIsCommittedClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Create an app list folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  top_level_grid->Layout();

  // Order apps grid to transition to temporary sort order.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Click on the folder item to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view = FindFolderItemView(top_level_grid);
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  EXPECT_TRUE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());

  // Rename folder to commit the sort order - verify that the folder remained
  // open.
  ash::AppListModelProvider::Get()->model()->delegate()->RequestFolderRename(
      folder_id, "Test folder");
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_TRUE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
  EXPECT_FALSE(app_list_test_api_.GetBubbleReorderUndoToastVisibility());
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       SortUsingContextMenuOnFolderChildViewTablet) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Create an app list folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  top_level_grid->Layout();

  // Click on the folder to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view = FindFolderItemView(top_level_grid);
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  ReorderByMouseClickAtContextMenuInAppsGrid(
      folder_grid, ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_FALSE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       FolderNotClosedIfTemporarySortIsCommittedTablet) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Create an app list folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  top_level_grid->Layout();

  // Order apps grid to transition to temporary sort order.
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Click on the folder item to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view = FindFolderItemView(top_level_grid);
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  EXPECT_TRUE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());

  // Rename folder to commit the sort order - verify that the folder remained
  // open.
  ash::AppListModelProvider::Get()->model()->delegate()->RequestFolderRename(
      folder_id, "Test folder");
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_TRUE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
  EXPECT_FALSE(app_list_test_api_.GetFullscreenReorderUndoToastVisibility());
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

  VerifyItemsVisibility();
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

  // Verify that the toast is under animation.
  EXPECT_TRUE(app_list_test_api_.GetToastContainerView()
                  ->layer()
                  ->GetAnimator()
                  ->is_animating());

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

  // Verify that the toast is under animation.
  EXPECT_TRUE(app_list_test_api_.GetToastContainerView()
                  ->layer()
                  ->GetAnimator()
                  ->is_animating());

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
                       DISABLED_TransitionToTabletModeDuringFadeOutAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  RegisterModeSwitchClosureOnFadeOutStarted(/*tablet_mode_enabled=*/true);
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListPageMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

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
// TODO(crbug.com/1302924): Flaky.
IN_PROC_BROWSER_TEST_F(
    AppListSortBrowserTest,
    DISABLED_TransitionToClamshellModeDuringFadeOutAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  RegisterModeSwitchClosureOnFadeOutStarted(/*tablet_mode_enabled=*/false);
  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kNameAlphabetical,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kFadeOutAborted);

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

// Verifies that changing an app's icon under color sort works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, SetIconUnderColorSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));

  // Set the app 3's icon color to be black.
  auto* model_updater = test::GetModelUpdater(AppListClientImpl::GetInstance());
  const syncer::StringOrdinal position_before_setting_black =
      model_updater->FindItem(app3_id_)->position();
  model_updater->FindItem(app3_id_)->SetIcon(
      CreateImageSkia(16, 16, SK_ColorBLACK), /*is_place_holder_icon=*/false);
  const syncer::StringOrdinal position_after_setting_black =
      model_updater->FindItem(app3_id_)->position();

  // Verify that the color order is still maintained.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app1_id_, app3_id_}));

  // Verify that the app 3's position changes.
  EXPECT_FALSE(
      position_after_setting_black.Equals(position_before_setting_black));

  // Set the app 3's icon color to be magenta.
  const std::vector<const ChromeAppListItem*> items_before_setting_magenta =
      model_updater->GetItems();
  model_updater->FindItem(app3_id_)->SetIcon(
      CreateImageSkia(16, 16, SK_ColorMAGENTA), /*is_place_holder_icon=*/false);

  // Verify that there is no position changes. Because after setting the app 3
  // should still be placed at the end.
  EXPECT_EQ(items_before_setting_magenta.size(), model_updater->ItemCount());
  for (const ChromeAppListItem* item : items_before_setting_magenta) {
    EXPECT_EQ(item->position(),
              model_updater->FindItem(item->id())->position());
  }

  // Set the app 1's icon color to be white. But the icon is labeled as a
  // placeholder.
  const std::vector<const ChromeAppListItem*> items_before_setting_white =
      model_updater->GetItems();
  model_updater->FindItem(app1_id_)->SetIcon(
      CreateImageSkia(16, 16, SK_ColorWHITE), /*is_place_holder_icon=*/true);

  // Verify that there is no position changes because setting a placeholder icon
  // should not update item positions.
  EXPECT_EQ(items_before_setting_white.size(), model_updater->ItemCount());
  for (const ChromeAppListItem* item : items_before_setting_white) {
    EXPECT_EQ(item->position(),
              model_updater->FindItem(item->id())->position());
  }
}

// Verifies color sort features by providing an app with the specified icon.
class AppListSortColorOrderBrowserTest : public AppListSortBrowserTest {
 public:
  AppListSortColorOrderBrowserTest() = default;
  AppListSortColorOrderBrowserTest(const AppListSortColorOrderBrowserTest&) =
      delete;
  AppListSortColorOrderBrowserTest& operator=(
      const AppListSortColorOrderBrowserTest&) = delete;
  ~AppListSortColorOrderBrowserTest() override = default;

  // AppListSortBrowserTest:
  void SetUpOnMainThread() override {
    AppListSortBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(extension_data_directory_.CreateUniqueTempDir());
    extension_path_ = SetUpFakeAppWithPureColorIcon(
        /*app_name=*/"yellow_app", /*icon_color=*/SK_ColorYELLOW);
  }

  base::FilePath extension_path_;

 private:
  // Sets up the resources of a fake app with the specified name and icon color.
  // Returns the path to the fake app data.
  base::FilePath SetUpFakeAppWithPureColorIcon(const std::string& app_name,
                                               SkColor icon_color) {
    // The export directory for an extension.
    const base::FilePath extension_path =
        extension_data_directory_.GetPath().Append(app_name);
    base::CreateDirectory(extension_path);

    // Prepare an icon file.
    constexpr char icon_file_name[] = "icon.png";
    base::FilePath icon_path = extension_path.AppendASCII(icon_file_name);
    base::File icon_file(icon_path,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);

    // Write the data of a circular icon in pure color into the icon file.
    constexpr int icon_size = 128;
    gfx::ImageSkia icon;
    icon = gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
        icon_size / 2, icon_color, icon);
    const sk_sp<SkImage> image = SkImage::MakeFromBitmap(*icon.bitmap());
    const sk_sp<SkData> png_data(
        image->encodeToData(SkEncodedImageFormat::kPNG, /*quality=*/100));
    icon_file.Write(0, (const char*)png_data->data(), png_data->size());
    icon_file.Close();

    // Prepare the app manifest file.
    base::FilePath manifest_path =
        extension_path.Append("manifest").AddExtension(".json");
    base::File manifest_file(manifest_path,
                             base::File::FLAG_CREATE | base::File::FLAG_WRITE);

    // Write data into the manifest file.
    char json_buffer[30];
    constexpr char icon_json[] = R"({"%d": "%s"})";
    base::strings::SafeSPrintf(json_buffer, icon_json, icon_size,
                               icon_file_name);
    char manifest_buffer[300];
    size_t count = base::strings::SafeSPrintf(manifest_buffer, kManifestData,
                                              app_name.c_str(), json_buffer);
    EXPECT_EQ(count, manifest_file.Write(0, manifest_buffer, count));
    manifest_file.Close();

    return extension_path;
  }

  // A temporary directory acting as a root directory for extension data.
  base::ScopedTempDir extension_data_directory_;
};

// Verify that installing an app under color sort works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortColorOrderBrowserTest,
                       InstallAppUnderColorSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  ReorderByMouseClickAtContextMenu(ash::AppListSortOrder::kColor,
                                   MenuType::kAppListNonFolderItemMenu,
                                   AnimationTargetStatus::kCompleted);

  std::string yellow_app_id = LoadExtension(extension_path_)->id();
  EXPECT_FALSE(yellow_app_id.empty());

  // Verify that the new app's position follows the color order.
  EXPECT_EQ(
      GetAppIdsInOrdinalOrder({app1_id_, app2_id_, app3_id_, yellow_app_id}),
      std::vector<std::string>({app2_id_, yellow_app_id, app3_id_, app1_id_}));
}

class AppListSortLoginTest
    : public ash::LoginManagerTest,
      public ::testing::WithParamInterface</*in_tablet=*/bool> {
 public:
  AppListSortLoginTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;

    feature_list_.InitWithFeatures(
        {ash::features::kProductivityLauncher, ash::features::kLauncherAppSort},
        /*disabled_features=*/{});
  }
  ~AppListSortLoginTest() override = default;

  void SetUpOnMainThread() override {
    ash::ShellTestApi().SetTabletModeEnabledForTest(GetParam());
    ash::LoginManagerTest::SetUpOnMainThread();
  }

  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, AppListSortLoginTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(AppListSortLoginTest,
                       RecordPrefSortOrderOnSessionStart) {
  // Verify that the pref sort order is recorded when a primary user logs in.
  base::HistogramTester histogram;
  LoginUser(account_id1_);
  const char* histogram_name =
      GetParam() ? ash::kTabletAppListSortOrderOnSessionStartHistogram
                 : ash::kClamshellAppListSortOrderOnSessionStartHistogram;
  histogram.ExpectBucketCount(histogram_name, ash::AppListSortOrder::kCustom,
                              1);

  // Verify that the pref sort order is recorded when a secondary user logs in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  histogram.ExpectBucketCount(histogram_name, ash::AppListSortOrder::kCustom,
                              2);

  // Switch back to the primary user. Verify that the pref sort order is not
  // recorded again.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  histogram.ExpectBucketCount(histogram_name, ash::AppListSortOrder::kCustom,
                              2);
}
