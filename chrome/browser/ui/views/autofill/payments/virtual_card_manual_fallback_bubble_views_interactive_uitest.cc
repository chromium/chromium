// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class VirtualCardManualFallbackBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest,
      public VirtualCardManualFallbackBubbleControllerImpl::ObserverForTest {
 public:
  // Various events that can be waited on by the DialogEventWaiter.
  enum class BubbleEvent : int {
    BUBBLE_SHOWN,
  };

  VirtualCardManualFallbackBubbleViewsInteractiveUiTest() = default;
  ~VirtualCardManualFallbackBubbleViewsInteractiveUiTest() override = default;
  VirtualCardManualFallbackBubbleViewsInteractiveUiTest(
      const VirtualCardManualFallbackBubbleViewsInteractiveUiTest&) = delete;
  VirtualCardManualFallbackBubbleViewsInteractiveUiTest& operator=(
      const VirtualCardManualFallbackBubbleViewsInteractiveUiTest&) = delete;

  // VirtualCardManualFallbackBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_)
      event_waiter_->OnEvent(BubbleEvent::BUBBLE_SHOWN);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    VirtualCardManualFallbackBubbleControllerImpl* controller =
        static_cast<VirtualCardManualFallbackBubbleControllerImpl*>(
            VirtualCardManualFallbackBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    DCHECK(controller);
    controller->SetEventObserverForTesting(this);
  }

  void ShowBubble() {
    CreditCard card = test::GetFullServerCard();
    ResetEventWaiterForSequence({BubbleEvent::BUBBLE_SHOWN});
    // Passing in empty image will fall back to use card network icon.
    GetController()->ShowBubble(&card,
                                /*virtual_card_cvc=*/u"123",
                                /*card_image=*/gfx::Image());
    event_waiter_->Wait();
  }

  void ReshowBubble() {
    ResetEventWaiterForSequence({BubbleEvent::BUBBLE_SHOWN});
    GetController()->ReshowBubble();
    event_waiter_->Wait();
  }

  void ClickOnViewAndWaitForBubbleDismissal(views::View* view) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetBubbleViews()->GetWidget());
    GetBubbleViews()->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(
            GetBubbleViews()->GetWidget()->non_client_view()->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetBubbleViews());
    EXPECT_TRUE(IsIconVisible());
  }

  void ClickOnView(views::View* view) {
    base::RunLoop closure_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        view, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        closure_loop.QuitClosure());
    closure_loop.Run();
  }

  bool IsIconVisible() { return GetIconView() && GetIconView()->GetVisible(); }

  VirtualCardManualFallbackBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  VirtualCardManualFallbackBubbleViews* GetBubbleViews() {
    VirtualCardManualFallbackBubbleControllerImpl* controller = GetController();
    if (!controller)
      return nullptr;

    return static_cast<VirtualCardManualFallbackBubbleViews*>(
        controller->GetBubble());
  }

  VirtualCardManualFallbackIconView* GetIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kVirtualCardManualFallback);
    DCHECK(icon);
    return static_cast<VirtualCardManualFallbackIconView*>(icon);
  }

  void ResetEventWaiterForSequence(std::list<BubbleEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<BubbleEvent>>(std::move(event_sequence));
  }

 private:
  std::unique_ptr<EventWaiter<BubbleEvent>> event_waiter_;
};

// Invokes a bubble showing the complete information for the virtual card
// selected to fill the form.
IN_PROC_BROWSER_TEST_F(VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
                       ShowBubble) {
  ShowBubble();
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());
}

// Invokes the bubble and verifies the bubble is dismissed upon page navigation.
IN_PROC_BROWSER_TEST_F(VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
                       DismissBubbleUponNavigation) {
  ShowBubble();
  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleViews()->GetWidget());
  ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com"));
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetBubbleViews());
  EXPECT_FALSE(GetIconView()->GetVisible());
}

// Disabled due to flakiness: crbug.com/1223042
IN_PROC_BROWSER_TEST_F(VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
                       DISABLED_Metrics_BubbleShownAndClosedByUser) {
  base::HistogramTester histogram_tester;

  ShowBubble();
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardManualFallbackBubble.Shown", false, 1);

  // Dismiss the bubble by clicking the close button.
  auto* close_button =
      GetBubbleViews()->GetBubbleFrameView()->GetCloseButtonForTesting();
  EXPECT_TRUE(close_button);
  ClickOnViewAndWaitForBubbleDismissal(close_button);

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardManualFallbackBubble.Result.FirstShow",
      AutofillMetrics::VirtualCardManualFallbackBubbleResultMetric::
          VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CLOSED,
      1);

  // Bubble is reshown by the user.
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardManualFallbackBubble.Shown", true, 1);

  // Dismiss the bubble by clicking the close button.
  close_button =
      GetBubbleViews()->GetBubbleFrameView()->GetCloseButtonForTesting();
  EXPECT_TRUE(close_button);
  ClickOnViewAndWaitForBubbleDismissal(close_button);

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardManualFallbackBubble.Result.Reshows",
      AutofillMetrics::VirtualCardManualFallbackBubbleResultMetric::
          VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CLOSED,
      1);

  // Bubble is reshown by the user.
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardManualFallbackBubble.Shown", true, 2);
}

IN_PROC_BROWSER_TEST_F(VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
                       Metrics_BubbleClosedByNotInteracted) {
  base::HistogramTester histogram_tester;

  // Show the bubble.
  ShowBubble();
  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  // Mock browser being closed.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleViews()->GetWidget());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_waiter.Wait();

  // Confirm metrics.
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardManualFallbackBubble.Result.FirstShow",
      AutofillMetrics::VirtualCardManualFallbackBubbleResultMetric::
          VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_NOT_INTERACTED,
      1);
}

IN_PROC_BROWSER_TEST_F(VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
                       Metrics_BubbleClosedByLostFocus) {
  base::HistogramTester histogram_tester;

  // Show the bubble.
  ShowBubble();
  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  // Mock deactivation due to lost focus.
  views::test::WidgetDestroyedWaiter destroyed_waiter1(
      GetBubbleViews()->GetWidget());
  GetBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);
  destroyed_waiter1.Wait();

  // Confirm .FirstShow metrics.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardManualFallbackBubble.Result.FirstShow",
      AutofillMetrics::VirtualCardManualFallbackBubbleResultMetric::
          VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_LOST_FOCUS,
      1);

  // Bubble is reshown by the user.
  ReshowBubble();

  // Mock deactivation due to lost focus.
  views::test::WidgetDestroyedWaiter destroyed_waiter2(
      GetBubbleViews()->GetWidget());
  GetBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);
  destroyed_waiter2.Wait();

  // Confirm .Reshows metrics.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardManualFallbackBubble.Result.Reshows",
      AutofillMetrics::VirtualCardManualFallbackBubbleResultMetric::
          VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_LOST_FOCUS,
      1);
}

}  // namespace autofill
