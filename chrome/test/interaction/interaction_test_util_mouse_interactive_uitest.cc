// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_mouse.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

class InteractionTestUtilMouseUiTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  InteractionTestUtilMouseUiTest() = default;
  ~InteractionTestUtilMouseUiTest() override = default;

  using Mouse = views::test::InteractionTestUtilMouse;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    mouse_ = std::make_unique<Mouse>(
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
    CHECK(mouse_->SetTouchMode(GetParam()));
  }

  void TearDownOnMainThread() override {
    mouse_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<Mouse> mouse_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE_P(TouchMode,
                         InteractionTestUtilMouseUiTest,
                         testing::Bool());
#else
INSTANTIATE_TEST_SUITE_P(TouchMode,
                         InteractionTestUtilMouseUiTest,
                         testing::Values(false));
#endif

IN_PROC_BROWSER_TEST_P(InteractionTestUtilMouseUiTest, MoveAndClick) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetContext(browser()->window()->GetElementContext())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Find the app menu button.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kToolbarAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [this](ui::InteractionSequence* seq,
                                  ui::TrackedElement* el) {
                             auto* const view =
                                 el->AsA<views::TrackedElementViews>()->view();
                             const gfx::Point pos =
                                 view->GetBoundsInScreen().CenterPoint();
                             // Perform the following gesture:
                             // - move to the center point of the app menu
                             // button
                             // - click the left mouse button
                             if (!mouse_->PerformGestures(
                                     view->GetWidget()->GetNativeWindow(),
                                     Mouse::MoveTo(pos),
                                     Mouse::Click(ui_controls::LEFT))) {
                               seq->FailForTesting();
                             }
                           })))
          // Verify that the click opened the app menu, which should contain a
          // known menu item.
          .AddStep(ui::InteractionSequence::StepBuilder().SetElementID(
              AppMenuModel::kMoreToolsMenuItem))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_P(InteractionTestUtilMouseUiTest, GestureAborted) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);

  auto cancel =
      base::BindLambdaForTesting([this]() { mouse_->CancelAllGestures(); });

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetContext(browser()->window()->GetElementContext())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Find the app menu button.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kToolbarAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [this, &cancel](ui::TrackedElement* el) {
                             auto* const view =
                                 el->AsA<views::TrackedElementViews>()->view();
                             const gfx::Point pos =
                                 view->GetBoundsInScreen().CenterPoint();
                             // Queue a cancellation. This should execute
                             // sometime after the mouse move is sent.
                             base::SingleThreadTaskRunner::GetCurrentDefault()
                                 ->PostTask(FROM_HERE, cancel);
                             // Perform the following gesture:
                             // - move to the center point of the app menu
                             // button
                             // - click the left mouse button
                             EXPECT_FALSE(mouse_->PerformGestures(
                                 view->GetWidget()->GetNativeWindow(),
                                 Mouse::MoveTo(pos),
                                 Mouse::Click(ui_controls::LEFT)));
                           })))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_P(InteractionTestUtilMouseUiTest, Drag) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);

  const GURL first_url =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL();
  const GURL kSecondUrl("chrome://version");
  ASSERT_TRUE(AddTabAtIndex(-1, kSecondUrl, ui::PAGE_TRANSITION_LINK));

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetContext(browser()->window()->GetElementContext())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Find the tab strip.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kTabStripElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* seq,
                          ui::TrackedElement* el) {
                        auto* const tab_strip = views::AsViewClass<TabStrip>(
                            el->AsA<views::TrackedElementViews>()->view());
                        // The second tab might still be animating in, which
                        // could cause weirdness if we try to drag.
                        tab_strip->StopAnimating(/* layout =*/true);

                        const gfx::Point start = tab_strip->tab_at(0)
                                                     ->GetBoundsInScreen()
                                                     .CenterPoint();
                        const gfx::Point end = tab_strip->tab_at(1)
                                                   ->GetBoundsInScreen()
                                                   .CenterPoint();
                        // Drag the first tab into the second spot.
                        if (!mouse_->PerformGestures(
                                tab_strip->GetWidget()->GetNativeWindow(),
                                Mouse::MoveTo(start),
                                Mouse::DragAndRelease(end))) {
                          seq->FailForTesting();
                        }
                      })))
          // When the gesture is complete, check that the gesture succeeded.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kTabStripElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* seq,
                          ui::TrackedElement* el) {
                        // Stop any remaining animations, and verify that the
                        // tab was moved.
                        auto* const tab_strip = views::AsViewClass<TabStrip>(
                            el->AsA<views::TrackedElementViews>()->view());
                        tab_strip->StopAnimating(/* layout =*/true);

                        EXPECT_EQ(kSecondUrl, browser()
                                                  ->tab_strip_model()
                                                  ->GetWebContentsAt(0)
                                                  ->GetURL());
                        EXPECT_EQ(first_url, browser()
                                                 ->tab_strip_model()
                                                 ->GetWebContentsAt(1)
                                                 ->GetURL());
                        // Clean up any drag gestures that have not yet properly
                        // cleaned up (this is a platform implementation issue
                        // for drag event handling).
                        mouse_->CancelAllGestures();
                      })))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
