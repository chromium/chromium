// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

typedef std::tuple<AutofillOfferData::OfferType>
    OfferNotificationBubbleViewsInteractiveUiTestData;

class OfferNotificationBubbleViewsInteractiveUiTest
    : public OfferNotificationBubbleViewsTestBase,
      public testing::WithParamInterface<
          OfferNotificationBubbleViewsInteractiveUiTestData> {
 public:
  OfferNotificationBubbleViewsInteractiveUiTest()
      : test_offer_type_(std::get<0>(GetParam())) {}

  ~OfferNotificationBubbleViewsInteractiveUiTest() override = default;
  OfferNotificationBubbleViewsInteractiveUiTest(
      const OfferNotificationBubbleViewsInteractiveUiTest&) = delete;
  OfferNotificationBubbleViewsInteractiveUiTest& operator=(
      const OfferNotificationBubbleViewsInteractiveUiTest&) = delete;

  void ShowBubbleForOfferAndVerify() {
    switch (test_offer_type_) {
      case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
        ShowBubbleForCardLinkedOfferAndVerify();
        break;
      case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
        ShowBubbleForFreeListingCouponOfferAndVerify();
        break;
      case AutofillOfferData::OfferType::UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  void ShowBubbleForCardLinkedOfferAndVerify() {
    NavigateTo(chrome::kChromeUINewTabPageURL);
    // Set the initial origin that the bubble will be displayed on.
    SetUpCardLinkedOfferDataWithDomains(
        {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateTo("https://www.example.com/first");
    WaitForObservedEvent();
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  void ShowBubbleForFreeListingCouponOfferAndVerify() {
    NavigateTo(chrome::kChromeUINewTabPageURL);
    // Set the initial origin that the bubble will be displayed on.
    SetUpFreeListingCouponOfferDataWithDomains(
        {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateTo("https://www.example.com/first");
    WaitForObservedEvent();
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  void CloseBubbleWithReason(views::Widget::ClosedReason closed_reason) {
    auto* widget = GetOfferNotificationBubbleViews()->GetWidget();
    EXPECT_TRUE(widget);
    views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
    widget->CloseWithReason(closed_reason);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetOfferNotificationBubbleViews());
    EXPECT_TRUE(IsIconVisible());
  }

  void SimulateClickOnIconAndReshowBubble() {
    auto* icon = GetOfferNotificationIconView();
    EXPECT_TRUE(icon);
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    chrome::ExecuteCommand(browser(), IDC_OFFERS_AND_REWARDS_FOR_PAGE);
    WaitForObservedEvent();
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  std::string GetSubhistogramNameForOfferType() const {
    switch (test_offer_type_) {
      case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
        return "CardLinkedOffer";
      case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
        return "FreeListingCouponOffer";
      case AutofillOfferData::OfferType::UNKNOWN:
        NOTREACHED();
        return std::string();
    }
  }

  const AutofillOfferData::OfferType test_offer_type_;
};

INSTANTIATE_TEST_SUITE_P(
    GpayCardLinked,
    OfferNotificationBubbleViewsInteractiveUiTest,
    testing::Values(AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER));
INSTANTIATE_TEST_SUITE_P(
    FreeListingCoupon,
    OfferNotificationBubbleViewsInteractiveUiTest,
    testing::Values(AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER));

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       Navigation) {
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
  SetUpOfferDataWithDomains(test_offer_type_, {GURL("https://www.example.com/"),
                                               GURL("https://www.test.com/")});

  for (const auto& test_case : test_cases) {
    NavigateTo(chrome::kChromeUINewTabPageURL);

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
IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       DismissBubble) {
  // Applies to card-linked offers only, as promo code offers do not have an OK
  // button.
  if (test_offer_type_ != AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER)
    return;

  ShowBubbleForOfferAndVerify();

  // Dismiss the bubble by clicking the ok button.
  CloseBubbleWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);

  // Navigates to another valid domain will not reshow the bubble.
  NavigateTo("https://www.example.com/second");
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  // Navigates to an invalid domain will dismiss the icon.
  NavigateTo("https://www.about.com/");
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_FALSE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_Shown) {
  base::HistogramTester histogram_tester;
  ShowBubbleForOfferAndVerify();

  histogram_tester.ExpectBucketCount("Autofill.OfferNotificationBubbleOffer." +
                                         GetSubhistogramNameForOfferType(),
                                     /*firstshow*/ false, 1);

  // Dismiss the bubble by clicking the close button.
  CloseBubbleWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  // Click on the omnibox icon to reshow the bubble.
  SimulateClickOnIconAndReshowBubble();

  histogram_tester.ExpectBucketCount("Autofill.OfferNotificationBubbleOffer." +
                                         GetSubhistogramNameForOfferType(),
                                     /*reshow*/ true, 1);
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_Acknowledged) {
  // Applies to card-linked offers only, as promo code offers do not have an OK
  // button.
  if (test_offer_type_ != AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER)
    return;

  base::HistogramTester histogram_tester;
  ShowBubbleForOfferAndVerify();

  // Dismiss the bubble by clicking the ok button.
  CloseBubbleWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED,
      1);

  // Click on the omnibox icon to reshow the bubble.
  SimulateClickOnIconAndReshowBubble();

  // Click on the ok button to dismiss the bubble.
  CloseBubbleWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".Reshows",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED,
      1);
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_Closed) {
  base::HistogramTester histogram_tester;
  ShowBubbleForOfferAndVerify();

  // Dismiss the bubble by clicking the close button.
  CloseBubbleWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_CLOSED,
      1);

  // Click on the omnibox icon to reshow the bubble.
  SimulateClickOnIconAndReshowBubble();

  // Click on the close button to dismiss the bubble.
  CloseBubbleWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".Reshows",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_CLOSED,
      1);
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_NotInteracted) {
  base::HistogramTester histogram_tester;
  ShowBubbleForOfferAndVerify();

  // Mock browser being closed.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetOfferNotificationBubbleViews()->GetWidget());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_waiter.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_NOT_INTERACTED,
      1);
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_LostFocus) {
  base::HistogramTester histogram_tester;
  ShowBubbleForOfferAndVerify();

  // Mock deactivation due to lost focus.
  CloseBubbleWithReason(views::Widget::ClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".FirstShow",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS,
      1);

  // Click on the omnibox icon to reshow the bubble.
  SimulateClickOnIconAndReshowBubble();

  // Mock deactivation due to lost focus.
  CloseBubbleWithReason(views::Widget::ClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".Reshows",
      AutofillMetrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS,
      1);
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       TooltipAndAccessibleName) {
  // Applies to promo code offers only, as card-linked offers do not have a
  // clickable promo code copy button.
  if (test_offer_type_ == AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER)
    return;

  ShowBubbleForOfferAndVerify();
  ASSERT_TRUE(GetOfferNotificationBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  std::u16string normal_button_tooltip = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_NORMAL);
  std::u16string clicked_button_tooltip = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_CLICKED);
  auto* promo_code_label_button =
      GetOfferNotificationBubbleViews()->promo_code_label_button_.get();
  EXPECT_EQ(normal_button_tooltip, promo_code_label_button->GetTooltipText());
  EXPECT_EQ(promo_code_label_button->GetText() + u" " + normal_button_tooltip,
            promo_code_label_button->GetAccessibleName());

  GetOfferNotificationBubbleViews()->OnPromoCodeButtonClicked();

  EXPECT_EQ(clicked_button_tooltip, promo_code_label_button->GetTooltipText());
  EXPECT_EQ(promo_code_label_button->GetText() + u" " + clicked_button_tooltip,
            promo_code_label_button->GetAccessibleName());
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       CopyPromoCode) {
  // Applies to promo code offers only, as card-linked offers do not have a
  // clickable promo code copy button.
  if (test_offer_type_ == AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER)
    return;

  ShowBubbleForOfferAndVerify();
  ASSERT_TRUE(GetOfferNotificationBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  // Simulate clicking on the copy promo code button.
  base::HistogramTester histogram_tester;
  GetOfferNotificationBubbleViews()->OnPromoCodeButtonClicked();

  // Clipboard should have the promo code text, which should be the same as what
  // is on the promo code button, and it should have logged the click.
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string clipboard_text;
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  std::u16string test_promo_code =
      base::ASCIIToUTF16(GetDefaultTestPromoCode());
  EXPECT_EQ(clipboard_text, test_promo_code);
  EXPECT_EQ(
      GetOfferNotificationBubbleViews()->promo_code_label_button_->GetText(),
      test_promo_code);
  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubblePromoCodeButtonClicked." +
          GetSubhistogramNameForOfferType(),
      true, 1);
}

}  // namespace autofill
