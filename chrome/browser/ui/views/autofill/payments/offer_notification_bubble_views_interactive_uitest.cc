// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"
#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"
#include "chrome/browser/ui/views/controls/subpage_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/payments/offer_notification_handler.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

struct OfferNotificationBubbleViewsInteractiveUiTestData {
  std::string name;
  AutofillOfferData::OfferType offer_type;
  std::optional<std::vector<base::test::FeatureRefAndParams>> enabled_features;
};

std::string GetTestName(
    const ::testing::TestParamInfo<
        OfferNotificationBubbleViewsInteractiveUiTestData>& info) {
  return info.param.name;
}

class OfferNotificationBubbleViewsInteractiveUiTest
    : public OfferNotificationBubbleViewsTestBase,
      public testing::WithParamInterface<
          OfferNotificationBubbleViewsInteractiveUiTestData> {
 public:
  OfferNotificationBubbleViewsInteractiveUiTest()
      : test_offer_type_(GetParam().offer_type) {
    if (GetParam().enabled_features.has_value()) {
      feature_list_.InitWithFeaturesAndParameters(
          GetParam().enabled_features.value(),
          /*disabled_features=*/{});
    }
  }

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
        break;
      case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
        ShowBubbleForGPayPromoCodeOfferAndVerify();
        break;
      case AutofillOfferData::OfferType::UNKNOWN:
        NOTREACHED();
    }
  }

  void ShowBubbleForCardLinkedOfferAndVerify() {
    NavigateTo(GURL(chrome::kChromeUINewTabPageURL));
    // Set the initial origin that the bubble will be displayed on.
    SetUpCardLinkedOfferDataWithDomains(
        {GetUrl("www.merchantsite1.test", "/"),
         GetUrl("www.merchantsite2.test", "/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateToAndWaitForForm(GetUrl("www.merchantsite1.test", "/first"));
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  void ShowBubbleForGPayPromoCodeOfferAndVerify() {
    NavigateTo(GURL(chrome::kChromeUINewTabPageURL));
    // Set the initial origin that the bubble will be displayed on.
    SetUpGPayPromoCodeOfferDataWithDomains(
        {GetUrl("www.merchantsite1.test", "/"),
         GetUrl("www.merchantsite2.test", "/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateToAndWaitForForm(GetUrl("www.merchantsite1.test", "/first"));
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  void CloseBubbleWithReason(views::Widget::ClosedReason closed_reason) {
    ASSERT_TRUE(GetOfferNotificationBubbleViews());
    auto* widget = GetOfferNotificationBubbleViews()->GetWidget();
    ASSERT_TRUE(widget);
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
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  std::string GetSubhistogramNameForOfferType() const {
    switch (test_offer_type_) {
      case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
        return "CardLinkedOffer";
      case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
        return "GPayPromoCodeOffer";
      case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
        return "FreeListingCouponOffer";
      case AutofillOfferData::OfferType::UNKNOWN:
        NOTREACHED();
    }
  }

  void ClearNotificationActiveDomainsForTesting() {
    GetOfferManager()
        ->notification_handler_.ClearShownNotificationIdForTesting();
  }

  TestAutofillClock test_clock_;
  const AutofillOfferData::OfferType test_offer_type_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/40228302): Split parameterized tests that are
// applicable for only one offer type.
INSTANTIATE_TEST_SUITE_P(
    GPayCardLinked,
    OfferNotificationBubbleViewsInteractiveUiTest,
    testing::Values(OfferNotificationBubbleViewsInteractiveUiTestData{
        "GPayCardLinked",
        AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER}));
INSTANTIATE_TEST_SUITE_P(
    GPayPromoCode,
    OfferNotificationBubbleViewsInteractiveUiTest,
    testing::Values(OfferNotificationBubbleViewsInteractiveUiTestData{
        "GPayPromoCode", AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER}));

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class OfferNotificationBubbleViewsInteractiveUiTestNoTestingConfig
    : public OfferNotificationBubbleViewsInteractiveUiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OfferNotificationBubbleViewsInteractiveUiTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};
INSTANTIATE_TEST_SUITE_P(
    GPayPromoCode,
    OfferNotificationBubbleViewsInteractiveUiTestNoTestingConfig,
    testing::Values(OfferNotificationBubbleViewsInteractiveUiTestData{
        "GPayPromoCode", AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER}));

// TODO(crbug.com/40817360): Flaky failures.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Navigation DISABLED_Navigation
#else
#define MAYBE_Navigation Navigation
#endif
IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsInteractiveUiTestNoTestingConfig,
    MAYBE_Navigation) {
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("http");

  static const struct {
    GURL url_navigated_to;
    bool bubble_should_be_visible;
  } test_cases[] = {
      // Different page on same domain keeps bubble.
      {GetUrl("www.merchantsite1.test", "/second/"), true},
      // Different domain not in offer's list dismisses bubble.
      {GetUrl("www.about.test", "/"), false},
      // Subdomain not in offer's list dismisses bubble.
      {GetUrl("support.merchantsite1.test", "/first/"), false},
      // http vs. https mismatch dismisses bubble.
      {GetUrl("www.merchantsite1.test", "/first/")
           .ReplaceComponents(replace_scheme),
       false},
      // Different domain in the offer's list keeps bubble.
      {GetUrl("www.merchantsite2.test", "/first/"), true},
  };

  // Set the initial origin that the bubble will be displayed on.
  SetUpOfferDataWithDomains(test_offer_type_,
                            {GetUrl("www.merchantsite1.test", "/"),
                             GetUrl("www.merchantsite2.test", "/")});

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StrCat(
        {test_case.url_navigated_to.spec(), ", bubble should be=",
         test_case.bubble_should_be_visible ? "visible" : "invisible"}));
    ClearNotificationActiveDomainsForTesting();
    NavigateTo(GURL(chrome::kChromeUINewTabPageURL));

    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateToAndWaitForForm(GetUrl("www.merchantsite1.test", "/first"));
    ASSERT_TRUE(WaitForObservedEvent());

    // Bubble should be visible.
    ASSERT_TRUE(IsIconVisible());
    ASSERT_TRUE(GetOfferNotificationBubbleViews());

    auto navigate = [&]() {
      // The test only spins up an HTTPS server, so there's no form to wait for
      // if it's a HTTP address.
      if (test_case.url_navigated_to.SchemeIs("https")) {
        NavigateToAndWaitForForm(test_case.url_navigated_to);
      } else {
        NavigateTo(test_case.url_navigated_to);
      }
    };

    // Navigate to a different url, and verify bubble/icon visibility.
    if (test_case.bubble_should_be_visible) {
      navigate();
    } else {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          GetOfferNotificationBubbleViews()->GetWidget());
      navigate();
      destroyed_waiter.Wait();
    }

    EXPECT_EQ(test_case.bubble_should_be_visible, IsIconVisible());
    EXPECT_EQ(test_case.bubble_should_be_visible,
              !!GetOfferNotificationBubbleViews());
  }
}

// Verifies the behavior of the offer notification bubble on different tabs.
// The steps are:
// 1. Creates the offer with two applicable merchant sites
// 2. Creates the setup that foreground tab is a blank website, the first
// background tab is merchant site 1, the second background tab is merchant
// site 2.
// 3. Checks that on the current blank site the offer notification bubble will
// not be shown.
// 4. Switches to the tab of merchant site 1. Makes sure the bubble and the icon
// are both visible.
// 5. Switches to the blank site. Makes sure the bubble and icon will be gone.
// 6. Switches to merchant site 2. Makes sure the icon is visible but the bubble
// is not, since we have shown the offer bubble in the tab of merchant site 1.
IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       CrossTabTracking) {
  SetUpOfferDataWithDomains(test_offer_type_,
                            {GetUrl("www.merchantsite1.test", "/"),
                             GetUrl("www.merchantsite2.test", "/")});

  // Makes sure the foreground tab is a blank site.
  NavigateTo(GURL("about:blank"));

  // Creates first background tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl("www.merchantsite1.test", "/"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  OfferNotificationBubbleControllerImpl* controller =
      static_cast<OfferNotificationBubbleControllerImpl*>(
          OfferNotificationBubbleController::GetOrCreate(
              browser()->tab_strip_model()->GetWebContentsAt(1)));
  ASSERT_TRUE(controller);
  AddEventObserverToController(controller);

  // Creates another merchant website in a second background tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl("www.merchantsite2.test", "/"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  controller = static_cast<OfferNotificationBubbleControllerImpl*>(
      OfferNotificationBubbleController::GetOrCreate(
          browser()->tab_strip_model()->GetWebContentsAt(2)));
  ASSERT_TRUE(controller);
  AddEventObserverToController(controller);

  // On current page notification should not be active.
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());

  // Change to the first background tab.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_TRUE(WaitForObservedEvent());
  // Icon should always be visible, and the bubble should be visible too.
  EXPECT_TRUE(IsIconVisible());
  ASSERT_TRUE(GetOfferNotificationBubbleViews());

  // Change back to the "original tab". The destroyed_waiter will wail until the
  // bubble is successfully dismissed and destroyed before proceeding with the
  // checks.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetOfferNotificationBubbleViews()->GetWidget());
  browser()->tab_strip_model()->ActivateTabAt(0);
  destroyed_waiter.Wait();
  // The icon and the bubble should not be visible.
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());

  // Change to the second background tab.
  browser()->tab_strip_model()->ActivateTabAt(2);
  // Icon should be visible and the bubble should not be visible.
  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

// Tests that bubble behaves correctly after user dismisses it.
IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       DismissBubble) {
  // Applies to card-linked offers only, as promo code offers do not have an OK
  // button.
  if (test_offer_type_ !=
      AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER) {
    return;
  }

  ShowBubbleForOfferAndVerify();

  // Dismiss the bubble by clicking the ok button.
  CloseBubbleWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);

  // Navigates to another valid domain will not reshow the bubble.
  NavigateToAndWaitForForm(GetUrl("www.merchantsite1.test", "/second"));
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  // Navigates to an invalid domain will dismiss the icon.
  NavigateToAndWaitForForm(GetUrl("www.about.test", "/"));
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
  if (test_offer_type_ !=
      AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER) {
    return;
  }

  base::HistogramTester histogram_tester;
  ShowBubbleForOfferAndVerify();

  // Dismiss the bubble by clicking the ok button.
  CloseBubbleWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".FirstShow",
      autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED,
      1);

  // Click on the omnibox icon to reshow the bubble.
  SimulateClickOnIconAndReshowBubble();

  // Click on the ok button to dismiss the bubble.
  CloseBubbleWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".Reshows",
      autofill_metrics::OfferNotificationBubbleResultMetric::
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
      autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_CLOSED,
      1);

  // Click on the omnibox icon to reshow the bubble.
  SimulateClickOnIconAndReshowBubble();

  // Click on the close button to dismiss the bubble.
  CloseBubbleWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".Reshows",
      autofill_metrics::OfferNotificationBubbleResultMetric::
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
      autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_NOT_INTERACTED,
      1);
}

// TODO(crbug.com/343099568): Flaky failures on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_Logging_LostFocus DISABLED_Logging_LostFocus
#else
#define MAYBE_Logging_LostFocus Logging_LostFocus
#endif
IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       MAYBE_Logging_LostFocus) {
  base::HistogramTester histogram_tester;
  ShowBubbleForOfferAndVerify();

  // Mock deactivation due to lost focus.
  CloseBubbleWithReason(views::Widget::ClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".FirstShow",
      autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS,
      1);

  // Click on the omnibox icon to reshow the bubble.
  SimulateClickOnIconAndReshowBubble();

  // Mock deactivation due to lost focus.
  CloseBubbleWithReason(views::Widget::ClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.OfferNotificationBubbleResult." +
          GetSubhistogramNameForOfferType() + ".Reshows",
      autofill_metrics::OfferNotificationBubbleResultMetric::
          OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS,
      1);
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       ShowGPayPromoCodeBubble) {
  // Applies to GPay promo code offers only.
  if (test_offer_type_ != AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER) {
    return;
  }

  ShowBubbleForOfferAndVerify();
  ASSERT_TRUE(GetOfferNotificationBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  auto* promo_code_styled_label =
      GetOfferNotificationBubbleViews()->promo_code_label_.get();
  auto* promo_code_usage_instructions_ =
      GetOfferNotificationBubbleViews()->instructions_label_.get();

  EXPECT_EQ(promo_code_styled_label->GetText(),
            base::ASCIIToUTF16(GetDefaultTestValuePropText()) + u" " +
                base::ASCIIToUTF16(GetDefaultTestSeeDetailsText()));
  EXPECT_EQ(promo_code_usage_instructions_->GetText(),
            base::ASCIIToUTF16(GetDefaultTestUsageInstructionsText()));

  // Simulate clicking on see details part of the text.
  GetOfferNotificationBubbleViews()->OnPromoCodeSeeDetailsClicked();
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(GetDefaultTestDetailsUrlString()));
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       ReshowOfferNotificationBubble_OfferDeletedBetweenShows) {
  // Applies to GPay promo code offers and card linked offers only.
  if (test_offer_type_ != AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER &&
      test_offer_type_ !=
          AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER) {
    return;
  }

  ShowBubbleForOfferAndVerify();
  ASSERT_TRUE(GetOfferNotificationBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  // Simulate the user closing the bubble.
  CloseBubbleWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

  // Simulate the user clearing server data.
  personal_data()->payments_data_manager().ClearAllServerDataForTesting();

  // Simulate the user re-showing the bubble by clicking on the icon.
  SimulateClickOnIconAndReshowBubble();
  ASSERT_TRUE(GetOfferNotificationBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  if (test_offer_type_ == AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER) {
    auto* promo_code_styled_label =
        GetOfferNotificationBubbleViews()->promo_code_label_.get();
    auto* promo_code_usage_instructions_ =
        GetOfferNotificationBubbleViews()->instructions_label_.get();

    EXPECT_EQ(promo_code_styled_label->GetText(),
              base::ASCIIToUTF16(GetDefaultTestValuePropText()) + u" " +
                  base::ASCIIToUTF16(GetDefaultTestSeeDetailsText()));
    EXPECT_EQ(promo_code_usage_instructions_->GetText(),
              base::ASCIIToUTF16(GetDefaultTestUsageInstructionsText()));

    // Simulate clicking on see details part of the text.
    GetOfferNotificationBubbleViews()->OnPromoCodeSeeDetailsClicked();
    EXPECT_EQ(
        browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
        GURL(GetDefaultTestDetailsUrlString()));
  }
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       IconViewAccessibleName) {
  EXPECT_EQ(
      GetOfferNotificationIconView()->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
  EXPECT_EQ(
      GetOfferNotificationIconView()->GetTextForTooltipAndAccessibleName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
}

}  // namespace autofill
