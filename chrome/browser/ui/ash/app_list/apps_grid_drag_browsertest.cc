// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"

class AppsGridDragBrowserTest : public InProcessBrowserTest {
 public:
  AppsGridDragBrowserTest() {}
  AppsGridDragBrowserTest(const AppsGridDragBrowserTest&) = delete;
  AppsGridDragBrowserTest& operator=(const AppsGridDragBrowserTest&) = delete;
  ~AppsGridDragBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Ensure that there are enough app items for reordering.
    test::PopulateDummyAppListItems(5);
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        browser()->window()->GetNativeWindow()->GetRootWindow());

    // Show the bubble launcher.
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::AcceleratorAction::kToggleAppList, {});

    app_list_test_api()->WaitForBubbleWindow(
        /*wait_for_opening_animation=*/true);
    root_apps_grid_test_api_ = std::make_unique<ash::test::AppsGridViewTestApi>(
        app_list_test_api()->GetTopLevelAppsGridView());

    ash::ShellTestApi().drag_drop_controller()->SetDisableNestedLoopForTesting(
        true);
  }

  // Starts mouse drag on the specified view.
  void StartAppListItemDrag(ash::AppListItemView* dragged_view) {
    event_generator_->MoveMouseTo(
        dragged_view->GetBoundsInScreen().CenterPoint());
    event_generator_->PressLeftButton();
    dragged_view->FireMouseDragTimerForTest();
  }

  // Returns a position between a pair of adjacent top level items that are
  // assumed to be aligned horizontally. `prev_item_index` indicates the least
  // index in the item pair.
  gfx::Point CalculatePositionBetweenAdjacentTopLevelItems(
      int prev_item_index) {
    const gfx::Rect prev_item_bounds_in_screen =
        root_apps_grid_test_api_
            ->GetViewAtVisualIndex(/*page=*/0, prev_item_index)
            ->GetBoundsInScreen();

    const views::View* next_view =
        root_apps_grid_test_api_->GetViewAtVisualIndex(/*page=*/0,
                                                       prev_item_index + 1);
    if (!next_view) {
      ADD_FAILURE() << "Item at slot 0, " << prev_item_index + 1
                    << " not found";
      return gfx::Point();
    }
    const gfx::Rect next_item_bounds_in_screen = next_view->GetBoundsInScreen();

    EXPECT_EQ(prev_item_bounds_in_screen.y(), next_item_bounds_in_screen.y());
    const int offset = (next_item_bounds_in_screen.OffsetFromOrigin() -
                        prev_item_bounds_in_screen.OffsetFromOrigin())
                           .x();
    const gfx::Point prev_item_center =
        prev_item_bounds_in_screen.CenterPoint();
    return gfx::Point(prev_item_center.x() + offset / 2, prev_item_center.y());
  }

  ash::AppListTestApi* app_list_test_api() { return &app_list_test_api_; }

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<ash::test::AppsGridViewTestApi> root_apps_grid_test_api_;

 private:
  ash::AppListTestApi app_list_test_api_;
};

// Verifies that reordering app list items by mouse drag works as expected on
// the bubble apps grid.
IN_PROC_BROWSER_TEST_F(AppsGridDragBrowserTest, ItemReorderByMouseDrag) {
  // Get the top level item ids before any operations.
  const std::vector<std::string> default_top_level_ids =
      app_list_test_api()->GetTopLevelViewIdList();

  // Start the mouse drag on the first item.
  ash::AppListItemView* dragged_view =
      root_apps_grid_test_api_->GetViewAtVisualIndex(/*page=*/0, /*slot=*/0);
  StartAppListItemDrag(dragged_view);

  // Move the first item to the location between the second one and the third
  // one.
  event_generator_->MoveMouseTo(
      CalculatePositionBetweenAdjacentTopLevelItems(/*prev_item_index=*/1));
  root_apps_grid_test_api_->FireReorderTimerAndWaitForAnimationDone();
  event_generator_->ReleaseLeftButton();

  // Verify that after drag-and-drop the first app and the second app swap
  // locations.
  std::vector<std::string> expected_top_level_ids = default_top_level_ids;
  std::swap(expected_top_level_ids[0], expected_top_level_ids[1]);
  EXPECT_EQ(expected_top_level_ids,
            app_list_test_api()->GetTopLevelViewIdList());
}

// Verifies that merging two items into a folder and moving an item out of a
// folder work as expected on the bubble apps grid.
IN_PROC_BROWSER_TEST_F(AppsGridDragBrowserTest, ItemMerge) {
  // Record the item count before any operations.
  const size_t default_top_level_item_count =
      app_list_test_api()->GetTopLevelViewIdList().size();

  base::RunLoop run_loop;
  app_list_test_api()->SetFolderViewAnimationCallback(run_loop.QuitClosure());

  // Merge the first item with the second one.
  StartAppListItemDrag(
      root_apps_grid_test_api_->GetViewAtVisualIndex(/*page=*/0, /*slot=*/0));
  event_generator_->MoveMouseTo(
      root_apps_grid_test_api_->GetViewAtVisualIndex(/*page=*/0, /*slot=*/1)
          ->GetBoundsInScreen()
          .CenterPoint());
  event_generator_->ReleaseLeftButton();
  root_apps_grid_test_api_->WaitForItemMoveAnimationDone();

  // The folder created by dragging one item on another should automatically
  // open the new folder - wait for the folder show animation to complete.
  run_loop.Run();
  EXPECT_FALSE(app_list_test_api()->IsFolderViewAnimating());

  // Verify that the top level item count decreases by one.
  EXPECT_EQ(default_top_level_item_count - 1,
            app_list_test_api()->GetTopLevelViewIdList().size());

  // Verify that the first item on the top level is a folder.
  ash::AppListItemView* folder_item =
      root_apps_grid_test_api_->GetViewAtVisualIndex(/*page=*/0, /*slot=*/0);
  EXPECT_TRUE(folder_item->is_folder());

  // Verify that the folder apps grid contains two items.
  ash::AppsGridView* folder_apps_grid_view =
      app_list_test_api()->GetFolderAppsGridView();
  EXPECT_EQ(2u, folder_apps_grid_view->view_model()->view_size());

  const std::vector<std::string> top_level_ids_after_merging =
      app_list_test_api()->GetTopLevelViewIdList();

  // Start the mouse drag on an item under the folder.
  ash::test::AppsGridViewTestApi folder_apps_grid_test_api(
      folder_apps_grid_view);
  ash::AppListItemView* dragged_view =
      folder_apps_grid_test_api.GetViewAtVisualIndex(/*page=*/0, /*slot=*/1);
  const std::string dragged_app_id = dragged_view->item()->id();
  StartAppListItemDrag(dragged_view);

  // Move the dragged item in two steps to ensure that the reordering animation
  // in the top level apps grid is triggered:
  // Step 1: move the dragged item upon a top level item (that is not occluded
  // by the folder view) then fire the reparenting timer.
  // Step 2: move the dragged item to the right of a top level item.
  event_generator_->MoveMouseTo(
      root_apps_grid_test_api_->GetViewAtVisualIndex(/*page=*/0, /*slot=*/2)
          ->GetBoundsInScreen()
          .CenterPoint());
  event_generator_->MoveMouseTo(
      CalculatePositionBetweenAdjacentTopLevelItems(/*prev_item_index=*/2));
  root_apps_grid_test_api_->FireReorderTimerAndWaitForAnimationDone();
  event_generator_->ReleaseLeftButton();

  // Calculate the expected top level item ids after moving `dragged_view` out
  // of the parent folder. `dragged_view` should be inserted at the fourth slot.
  std::vector<std::string> expected_top_level_ids = top_level_ids_after_merging;
  expected_top_level_ids.insert(expected_top_level_ids.begin() + 3,
                                dragged_app_id);

  const std::vector<std::string> final_top_level_ids =
      app_list_test_api()->GetTopLevelViewIdList();
  EXPECT_EQ(expected_top_level_ids, final_top_level_ids);

  // Verify that after reparenting the top level item count is equal to the
  // default value.
  EXPECT_EQ(default_top_level_item_count, final_top_level_ids.size());
}
