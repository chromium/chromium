// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/mouse/interaction_test_util_mouse.h"

#include <functional>
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
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_specifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

class InteractionTestUtilMouseUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  InteractionTestUtilMouseUiTest() = default;
  ~InteractionTestUtilMouseUiTest() override = default;

  using Mouse = views::test::InteractionTestUtilMouse;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    CHECK(mouse_util().SetTouchMode(GetParam()));
  }

  void TearDownOnMainThread() override {
    ResetLayoutStepState();
    InteractiveBrowserTest::TearDownOnMainThread();
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

  auto StopTabStripAnimationsAndLayout() {
    return Do([this]() {
             auto* const browser_view =
                 BrowserView::GetBrowserViewForBrowser(browser());
             TabStripRegionView* const region_view =
                 browser_view->tab_strip_view();
             if (auto* const base_region =
                     views::AsViewClass<BaseTabStripRegionView>(region_view)) {
               // Flush pending layout first so
               // TabCollectionAnimatingLayoutManager calculates the target
               // layout and starts any queued animation before ending
               // animations; then re-layout so it applies the final target
               // bounds immediately.
               if (views::Widget* widget = base_region->GetWidget()) {
                 widget->LayoutRootViewIfNecessary();
               }
               base_region->GetTabStripCollectionController()
                   ->animation_coordinator()
                   .AnimationEnded(nullptr);
             } else {
               browser_view->horizontal_tab_strip_for_testing()
                   ->StopAnimating();
             }
             if (views::Widget* widget = region_view->GetWidget()) {
               widget->LayoutRootViewIfNecessary();
             }
           })
        .SetDescription("StopTabStripAnimationsAndLayout");
  }

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
  RunTestSequence(
      // Wait for the app menu button layout to settle.
      WaitForAppMenuButtonLayout(),
      // Find the app menu button.
      WithElement(
          kToolbarAppMenuButtonElementId,
          [this](ui::TrackedElement* el) {
            const gfx::Point pos = el->GetScreenBounds().CenterPoint();
            // Perform the following gesture:
            // - move to the center point of the app menu button
            // - click the left mouse button
            EXPECT_TRUE(mouse_util().PerformGestures(
                Mouse::GestureParams(browser()->GetWindow()->GetNativeWindow()),
                Mouse::MoveTo(pos), Mouse::Click(ui_controls::LEFT)));
          }),
      // Verify that the click opened the app menu, which should contain a
      // known menu item.
      WaitForShow(AppMenuModel::kMoreToolsMenuItem));
}

IN_PROC_BROWSER_TEST_P(InteractionTestUtilMouseUiTest, GestureAborted) {
  auto cancel = base::BindLambdaForTesting(
      [this]() { mouse_util().CancelAllGestures(); });

  RunTestSequence(
      // Wait for the app menu button layout to settle.
      WaitForAppMenuButtonLayout(),
      // Find the app menu button.
      WithElement(kToolbarAppMenuButtonElementId, [this, &cancel](
                                                      ui::TrackedElement* el) {
        const gfx::Point pos = el->GetScreenBounds().CenterPoint();
        // Queue a cancellation. This should execute
        // sometime after the mouse move is sent.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                                    cancel);
        // Perform the following gesture:
        // - move to the center point of the app menu button
        // - click the left mouse button
        EXPECT_FALSE(mouse_util().PerformGestures(
            Mouse::GestureParams(browser()->GetWindow()->GetNativeWindow()),
            Mouse::MoveTo(pos), Mouse::Click(ui_controls::LEFT)));
      }));
}

IN_PROC_BROWSER_TEST_P(InteractionTestUtilMouseUiTest, Drag) {
  const GURL first_url =
      browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL();
  const GURL kSecondUrl("chrome://version");
  ASSERT_TRUE(AddTabAtIndex(-1, kSecondUrl, ui::PAGE_TRANSITION_LINK));

  gfx::Point start;
  gfx::Point end;
  RunTestSequence(
      // The second tab might still be animating in, which
      // could cause weirdness if we try to drag.
      StopTabStripAnimationsAndLayout(),
      // Find the tab strip and calculate drag start and end positions.
      WithView(
          kTabStripRegionElementId,
          [&](TabStripRegionView* tab_strip_region) {
            TabStripModel* const model = browser()->GetTabStripModel();
            start = tab_strip_region
                        ->GetTabAnchorView(model->GetTabAtIndex(0)->GetHandle())
                        ->GetBoundsInScreen()
                        .CenterPoint();
            end = tab_strip_region
                      ->GetTabAnchorView(model->GetTabAtIndex(1)->GetHandle())
                      ->GetBoundsInScreen()
                      .CenterPoint();
          }),
      // Drag the first tab into the second spot.
      MoveMouseTo(std::ref(start)), DragMouseTo(std::ref(end)),
      // Stop any remaining animations, and verify that the tab was moved.
      StopTabStripAnimationsAndLayout(),
      CheckResult(
          [this]() {
            return browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL();
          },
          kSecondUrl),
      CheckResult(
          [this]() {
            return browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL();
          },
          first_url));
}
