// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/mouse/interaction_test_util_mouse.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_specifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

class InteractionTestUtilMouseUiTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  InteractionTestUtilMouseUiTest() = default;
  ~InteractionTestUtilMouseUiTest() override = default;

  using Mouse = views::test::InteractionTestUtilMouse;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    mouse_ = std::make_unique<Mouse>(browser()->GetWindow()->GetNativeWindow());
    CHECK(mouse_->SetTouchMode(GetParam()));
  }

  void TearDownOnMainThread() override {
    ResetLayoutStepState();
    mouse_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Builds a sequence step that waits until the app menu button's screen bounds
  // and any WebUI toolbar compositor frame have settled before targeting it.
  ui::InteractionSequence::StepBuilder WaitForAppMenuButtonLayout() {
    return std::move(
        ui::InteractionSequence::StepBuilder()
            .SetDescription("WaitForAppMenuButtonLayout")
            .SetElement(kToolbarAppMenuButtonElementId)
            .SetStartCallback(base::BindLambdaForTesting(
                [this](ui::InteractionSequence* seq, ui::TrackedElement* el) {
                  if (el->IsA<views::TrackedElementViews>() &&
                      IsAppMenuButtonLaidOut(el)) {
                    return;
                  }
                  layout_step_handle_ = seq->SeizeStepTransitionControl();
                  CheckAppMenuButtonLayout();
                }))
            .SetEndCallback(base::BindLambdaForTesting(
                [this]() { ResetLayoutStepState(); })));
  }

  std::unique_ptr<Mouse> mouse_;

 private:
  // Stops polling and releases any held step transition handle.
  void ResetLayoutStepState() {
    layout_poll_timer_.Stop();
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (layout_step_handle_) {
      std::move(layout_step_handle_).Proceed(/*success=*/false);
    }
  }

  // Returns true if the app menu button has non-empty bounds near the edge.
  bool IsAppMenuButtonLaidOut(ui::TrackedElement* el) {
    if (!el) {
      return false;
    }
    const gfx::Rect bounds = el->GetScreenBounds();
    if (bounds.width() <= 1 || bounds.height() <= 1) {
      return false;
    }
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    const gfx::Rect browser_bounds = browser_view->GetBoundsInScreen();
    return base::i18n::IsRTL()
               ? bounds.x() <= browser_bounds.x() + bounds.width()
               : bounds.right() >= browser_bounds.right() - bounds.width();
  }

  // Checks whether the app menu button is laid out and its compositor frame
  // ready.
  void CheckAppMenuButtonLayout() {
    if (!layout_step_handle_) {
      return;
    }
    auto* const el = BrowserElements::From(browser())->GetElement(
        kToolbarAppMenuButtonElementId);
    if (el && IsAppMenuButtonLaidOut(el)) {
      if (el->IsA<views::TrackedElementViews>()) {
        OnAppMenuButtonLayoutReady();
        return;
      }
      if (auto* const web_ui_el = el->AsA<ui::TrackedElementWebUI>()) {
        views::WebView* const web_view = web_ui_el->GetWebView();
        content::WebContents* const web_contents =
            web_view
                ? web_view->GetWebContents()
                : (web_ui_el->handler() ? web_ui_el->handler()->web_contents()
                                        : nullptr);
        if (web_contents && web_contents->GetRenderWidgetHostView()) {
          content::NotifyCopyableViewInWebContents(
              web_contents,
              base::BindOnce(
                  &InteractionTestUtilMouseUiTest::OnAppMenuButtonLayoutReady,
                  weak_ptr_factory_.GetWeakPtr()));
          return;
        }
      }
    }
    layout_poll_timer_.Start(
        FROM_HERE, base::Milliseconds(/*n=*/10),
        base::BindOnce(
            &InteractionTestUtilMouseUiTest::CheckAppMenuButtonLayout,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Resumes the interaction sequence once the button layout is ready.
  void OnAppMenuButtonLayoutReady() {
    layout_poll_timer_.Stop();
    if (layout_step_handle_) {
      std::move(layout_step_handle_).Proceed(/*success=*/true);
    }
  }

  base::OneShotTimer layout_poll_timer_;
  ui::InteractionSequence::StepTransitionHandle layout_step_handle_;
  base::WeakPtrFactory<InteractionTestUtilMouseUiTest> weak_ptr_factory_{this};
};

#if BUILDFLAG(IS_CHROMEOS)
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
          .SetContext(BrowserElements::From(browser())->GetContext())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Wait for the app menu button layout to settle.
          .AddStep(WaitForAppMenuButtonLayout())
          // Find the app menu button.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kToolbarAppMenuButtonElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [this](ui::InteractionSequence* seq,
                             ui::TrackedElement* el) {
                        const gfx::Point pos =
                            el->GetScreenBounds().CenterPoint();
                        // Perform the following gesture:
                        // - move to the center point of the app menu
                        // button
                        // - click the left mouse button
                        if (!mouse_->PerformGestures(
                                Mouse::GestureParams(
                                    browser()->GetWindow()->GetNativeWindow()),
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
          .SetContext(BrowserElements::From(browser())->GetContext())
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          // Wait for the app menu button layout to settle.
          .AddStep(WaitForAppMenuButtonLayout())
          // Find the app menu button.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kToolbarAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [this, &cancel](ui::TrackedElement* el) {
                             const gfx::Point pos =
                                 el->GetScreenBounds().CenterPoint();
                             // Queue a cancellation. This should execute
                             // sometime after the mouse move is sent.
                             base::SingleThreadTaskRunner::GetCurrentDefault()
                                 ->PostTask(FROM_HERE, cancel);
                             // Perform the following gesture:
                             // - move to the center point of the app menu
                             // button
                             // - click the left mouse button
                             EXPECT_FALSE(mouse_->PerformGestures(
                                 Mouse::GestureParams(
                                     browser()->GetWindow()->GetNativeWindow()),
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
      browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL();
  const GURL kSecondUrl("chrome://version");
  ASSERT_TRUE(AddTabAtIndex(-1, kSecondUrl, ui::PAGE_TRANSITION_LINK));

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetContext(BrowserElements::From(browser())->GetContext())
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
                        tab_strip->StopAnimating();

                        const gfx::Point start = tab_strip->tab_at(0)
                                                     ->GetBoundsInScreen()
                                                     .CenterPoint();
                        const gfx::Point end = tab_strip->tab_at(1)
                                                   ->GetBoundsInScreen()
                                                   .CenterPoint();
                        // Drag the first tab into the second spot.
                        if (!mouse_->PerformGestures(
                                Mouse::GestureParams(
                                    tab_strip->GetWidget()->GetNativeWindow()),
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
                        tab_strip->StopAnimating();

                        EXPECT_EQ(kSecondUrl, browser()
                                                  ->GetTabStripModel()
                                                  ->GetWebContentsAt(0)
                                                  ->GetURL());
                        EXPECT_EQ(first_url, browser()
                                                 ->GetTabStripModel()
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
