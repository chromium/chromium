// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include <string.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/test_support/app_menu_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"

// This test ensures basic compatibility of InteractionSequence[Views] and a
// live browser. It verifies that a simple journey of opening the main menu and
// identifying a specific menu item works.
//
// Located in user_education because it serves as the foundation for Tutorials
// (along with being the primary use case).
//
// In the future, we should add additional specific cases, such as opening a
// dialog or multiple submenus.
class InteractionSequenceUiTest : public InProcessBrowserTest {
 public:
  InteractionSequenceUiTest() {
    test_util_.AddSimulator(
        std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
    InteractionTestUtilBrowser::PopulateSimulators(test_util_);
  }

  ~InteractionSequenceUiTest() override = default;

  static void ClearEventQueue() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  // Simulates pressing `element` and verifies that the action succeeded.
  void PressButton(ui::TrackedElement* element) {
    EXPECT_EQ(ui::test::ActionResult::kSucceeded,
              test_util_.PressButton(element));
  }

  // Dispatches framework-agnostic UI actions across Views and WebUI elements.
  ui::test::InteractionTestUtil test_util_;
};

IN_PROC_BROWSER_TEST_F(InteractionSequenceUiTest, OpenMainMenuAndViewHelpItem) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);

  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForView(
          BrowserView::GetBrowserViewForBrowser(browser()));

  WaitForInitialWebUIToolbar(browser());

  // Demonstrate that we can find the app button without having to pick through
  // the BrowserView hierarchy.
  ui::TrackedElement* app_menu_button = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    app_menu_button =
        ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
            kToolbarAppMenuButtonElementId, context);
    return app_menu_button != nullptr;
  }));

  // Define a simple sequence of:
  // - spotting the app menu button
  // - clicking the app menu button
  // - observing a submenu item
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(ui::InteractionSequence::WithInitialElement(app_menu_button))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kToolbarAppMenuButtonElementId)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(AppMenuModel::kHistoryMenuItem)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .Build())
          .Build();

  // Start the sequence and verify that it does not proceed.
  sequence->Start();
  ClearEventQueue();

  // Click the app menu button, displaying the target element.
  EXPECT_ASYNC_CALL_IN_SCOPE(completed, Run, PressButton(app_menu_button));

  // Verify that we found the correct element and it is visible.
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      AppMenuModel::kHistoryMenuItem, context));
  const int history_menu_item_id =
      base::FeatureList::IsEnabled(features::kAppMenuGlowUp)
          ? static_cast<int>(kActionRecentTabsSubmenu)
          : AppMenuModel::kRecentTabsMenuPlaceholder;
  views::MenuItemView* const history_menu_item =
      AppMenuTestAccessor(browser()).GetRootMenuItemView()->GetMenuItemByID(
          history_menu_item_id);
  EXPECT_EQ(history_menu_item,
            views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                AppMenuModel::kHistoryMenuItem, context));
}
