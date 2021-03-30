// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class OfferNotificationBubbleViewsInteractiveUiTest
    : public OfferNotificationBubbleViewsTestBase {
 public:
  OfferNotificationBubbleViewsInteractiveUiTest() = default;
  ~OfferNotificationBubbleViewsInteractiveUiTest() override = default;
  OfferNotificationBubbleViewsInteractiveUiTest(
      const OfferNotificationBubbleViewsInteractiveUiTest&) = delete;
  OfferNotificationBubbleViewsInteractiveUiTest& operator=(
      const OfferNotificationBubbleViewsInteractiveUiTest&) = delete;

  void ShowBubbleAndVerify() {
    NavigateTo(chrome::kChromeUINewTabURL);
    // Set the initial origin that the bubble will be displayed on.
    SetUpOfferDataWithDomains(
        {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateTo("https://www.example.com/first");
    WaitForObservedEvent();
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  void ClickOnViewAndWaitForBubbleDismissal(views::View* view) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetOfferNotificationBubbleViews()->GetWidget());
    GetOfferNotificationBubbleViews()->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(GetOfferNotificationBubbleViews()
                                                 ->GetWidget()
                                                 ->non_client_view()
                                                 ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetOfferNotificationBubbleViews());
    EXPECT_TRUE(IsIconVisible());
  }

  void ClickOnIconAndReshowBubble() {
    auto* icon = GetOfferNotificationIconView();
    EXPECT_TRUE(icon);
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    ClickOnView(icon);
    WaitForObservedEvent();
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  // TODO(crbug.com/1181615): Move shared functions to some utils.
  void ClickOnView(views::View* view) {
    base::RunLoop closure_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        view, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        closure_loop.QuitClosure());
    closure_loop.Run();
  }
};

// Flaky on Linux. crbug.com/1182526
#if defined(OS_LINUX)
#define MAYBE_Navigation DISABLED_Navigation
#else
#define MAYBE_Navigation Navigation
#endif
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       MAYBE_Navigation) {
  static const struct {
    std::string url_navigated_to;
    bool bubble_should_be_visible;
  } test_cases[] = {
      // Different page on same domain keeps bubble.
      {"https://www.example.com/second/", true},
      // Different domain not in offer's list dismisses bubble.
      {"https://www.about.com/", false},
      // Subdomain not in offer's list dismisses bubble.
      {"https://support.example.com/first/", false},
      // http vs. https mismatch dismisses bubble.
      {"http://www.example.com/first/", false},
      // Different domain in the offer's list keeps bubble.
      {"https://www.test.com/first/", true},
  };

  // Set the initial origin that the bubble will be displayed on.
  SetUpOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});

  for (const auto& test_case : test_cases) {
    NavigateTo(chrome::kChromeUINewTabURL);

    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateTo("https://www.example.com/first");
    WaitForObservedEvent();

    // Bubble should be visible.
    ASSERT_TRUE(IsIconVisible());
    ASSERT_TRUE(GetOfferNotificationBubbleViews());

    // Navigate to a different url, and verify bubble/icon visibility.
    if (test_case.bubble_should_be_visible) {
      NavigateTo(test_case.url_navigated_to);
    } else {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          GetOfferNotificationBubbleViews()->GetWidget());
      NavigateTo(test_case.url_navigated_to);
      destroyed_waiter.Wait();
    }

    EXPECT_EQ(test_case.bubble_should_be_visible, IsIconVisible());
    EXPECT_EQ(test_case.bubble_should_be_visible,
              !!GetOfferNotificationBubbleViews());
  }
}

// Tests that bubble behaves correctly after user dismisses it.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       DismissBubble) {
  ShowBubbleAndVerify();

  // Dismiss the bubble by clicking the ok button.
  auto* ok_button = GetOfferNotificationBubbleViews()->GetOkButton();
  EXPECT_TRUE(ok_button);
  ClickOnViewAndWaitForBubbleDismissal(ok_button);

  // Navigates to another valid domain will not reshow the bubble.
  NavigateTo("https://www.example.com/second");
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  // Navigates to an invalid domain will dismiss the icon.
  NavigateTo("https://www.about.com/");
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_FALSE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_Shown) {
  base::HistogramTester histogram_tester;
  ShowBubbleAndVerify();

  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubbleOffer.CardLinkedOffer",
      /*firstshow*/ false, 1);

  // Dismiss the bubble by clicking the ok button.
  auto* ok_button = GetOfferNotificationBubbleViews()->GetOkButton();
  EXPECT_TRUE(ok_button);
  ClickOnViewAndWaitForBubbleDismissal(ok_button);

  // Click on the omnibox icon to reshow the bubble.
  ClickOnIconAndReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubbleOffer.CardLinkedOffer", /*reshow*/ true,
      1);
}

#if defined(OS_MAC)
// OS_MAC: Flaky: https://crbug.com/1186169.
#define MAYBE_Logging_Acknowledged DISABLED_Logging_Acknowledged
#else
#define MAYBE_Logging_Acknowledged Logging_Acknowledged
#endif
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       MAYBE_Logging_Acknowledged) {
  base::HistogramTester histogram_tester;
  ShowBubbleAndVerify();

  // Dismiss the bubble by clicking the ok button.
  auto* ok_button = GetOfferNotificationBubbleViews()->GetOkButton();
  EXPECT_TRUE(ok_button);
  ClickOnViewAndWaitForBubbleDismissal(ok_button);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult.CardLinkedOffer.FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED,
      1);

  // Click on the omnibox icon to reshow the bubble.
  ClickOnIconAndReshowBubble();

  // Click on the ok button to dismiss the bubble.
  ok_button = GetOfferNotificationBubbleViews()->GetOkButton();
  EXPECT_TRUE(ok_button);
  ClickOnViewAndWaitForBubbleDismissal(ok_button);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult.CardLinkedOffer.Reshows",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED,
      1);
}

#if defined(OS_MAC)
// OS_MAC: Flaky: https://crbug.com/1186164.
#define MAYBE_Logging_Closed DISABLED_Logging_Closed
#else
#define MAYBE_Logging_Closed Logging_Closed
#endif
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       MAYBE_Logging_Closed) {
  base::HistogramTester histogram_tester;
  ShowBubbleAndVerify();

  // Dismiss the bubble by clicking the close button.
  auto* close_button = GetOfferNotificationBubbleViews()
                           ->GetBubbleFrameView()
                           ->GetCloseButtonForTesting();
  EXPECT_TRUE(close_button);
  ClickOnViewAndWaitForBubbleDismissal(close_button);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult.CardLinkedOffer.FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_CLOSED,
      1);

  // Click on the omnibox icon to reshow the bubble.
  ClickOnIconAndReshowBubble();

  // Click on the close button to dismiss the bubble.
  close_button = GetOfferNotificationBubbleViews()
                     ->GetBubbleFrameView()
                     ->GetCloseButtonForTesting();
  EXPECT_TRUE(close_button);
  ClickOnViewAndWaitForBubbleDismissal(close_button);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult.CardLinkedOffer.Reshows",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_CLOSED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_NotInteracted) {
  base::HistogramTester histogram_tester;
  ShowBubbleAndVerify();

  // Mock browser being closed.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetOfferNotificationBubbleViews()->GetWidget());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_waiter.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult.CardLinkedOffer.FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_NOT_INTERACTED,
      1);
}

IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_LostFocus) {
  base::HistogramTester histogram_tester;
  ShowBubbleAndVerify();

  // Mock deactivation due to lost focus.
  views::test::WidgetDestroyedWaiter destroyed_waiter1(
      GetOfferNotificationBubbleViews()->GetWidget());
  GetOfferNotificationBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);
  destroyed_waiter1.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult.CardLinkedOffer.FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS,
      1);

  // Click on the omnibox icon to reshow the bubble.
  ClickOnIconAndReshowBubble();

  // Mock deactivation due to lost focus.
  views::test::WidgetDestroyedWaiter destroyed_waiter2(
      GetOfferNotificationBubbleViews()->GetWidget());
  GetOfferNotificationBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);
  destroyed_waiter2.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult.CardLinkedOffer.Reshows",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS,
      1);
}

}  // namespace autofill
