// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"
#include "chrome/browser/ui/views/autofill/payments/promo_code_label_view.h"
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
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

struct OfferNotificationBubbleViewsInteractiveUiTestData {
  std::string name;
  AutofillOfferData::OfferType offer_type;
  absl::optional<std::vector<base::test::FeatureRefAndParams>> enabled_features;
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

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &OfferNotificationBubbleViewsInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

  void ShowBubbleForOfferAndVerify() {
    switch (test_offer_type_) {
      case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
        ShowBubbleForCardLinkedOfferAndVerify();
        break;
      case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
        ShowBubbleForFreeListingCouponOfferAndVerify();
        break;
      case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
        ShowBubbleForGPayPromoCodeOfferAndVerify();
        break;
      case AutofillOfferData::OfferType::UNKNOWN:
        NOTREACHED_NORETURN();
    }
  }

  void ShowBubbleForCardLinkedOfferAndVerify() {
    NavigateTo(GURL(chrome::kChromeUINewTabPageURL));
    // Set the initial origin that the bubble will be displayed on.
    SetUpCardLinkedOfferDataWithDomains({GetUrl("www.merchantsite1.com", "/"),
                                         GetUrl("www.merchantsite2.com", "/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateToAndWaitForForm(GetUrl("www.merchantsite1.com", "/first"));
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  void ShowBubbleForFreeListingCouponOfferAndVerify() {
    NavigateTo(GURL(chrome::kChromeUINewTabPageURL));
    // Set the initial origin that the bubble will be displayed on.
    SetUpFreeListingCouponOfferDataWithDomains(
        {GetUrl("www.merchantsite1.com", "/"),
         GetUrl("www.merchantsite2.com", "/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateToAndWaitForForm(GetUrl("www.merchantsite1.com", "/first"));
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());
  }

  void ShowBubbleForGPayPromoCodeOfferAndVerify() {
    NavigateTo(GURL(chrome::kChromeUINewTabPageURL));
    // Set the initial origin that the bubble will be displayed on.
    SetUpGPayPromoCodeOfferDataWithDomains(
        {GetUrl("www.merchantsite1.com", "/"),
         GetUrl("www.merchantsite2.com", "/")});
    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateToAndWaitForForm(GetUrl("www.merchantsite1.com", "/first"));
    ASSERT_TRUE(WaitForObservedEvent());
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
        NOTREACHED_NORETURN();
    }
  }

  void ClearNotificationActiveDomainsForTesting() {
    GetOfferManager()
        ->notification_handler_.ClearShownNotificationIdForTesting();
  }

  TestAutofillClock test_clock_;
  const AutofillOfferData::OfferType test_offer_type_;
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// TODO(https://crbug.com/1334806): Split parameterized tests that are
// applicable for only one offer type.
INSTANTIATE_TEST_SUITE_P(
    GPayCardLinked,
    OfferNotificationBubbleViewsInteractiveUiTest,
    testing::Values(OfferNotificationBubbleViewsInteractiveUiTestData{
        "GPayCardLinked",
        AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER}));
INSTANTIATE_TEST_SUITE_P(
    FreeListingCoupon,
    OfferNotificationBubbleViewsInteractiveUiTest,
    testing::Values(
        OfferNotificationBubbleViewsInteractiveUiTestData{
            "FreeListingCoupon_default",
            AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER},
        OfferNotificationBubbleViewsInteractiveUiTestData{
            "FreeListingCoupon_on_navigation",
            AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER,
            absl::make_optional<std::vector<base::test::FeatureRefAndParams>>(
                {{commerce::kShowDiscountOnNavigation, {}}})},
        OfferNotificationBubbleViewsInteractiveUiTestData{
            "FreeListingCoupon_on_navigation_chrome_refresh_style",
            AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER,
            absl::make_optional<std::vector<base::test::FeatureRefAndParams>>(
                {{commerce::kShowDiscountOnNavigation, {}},
                 {::features::kChromeRefresh2023, {}}})}),
    GetTestName);
INSTANTIATE_TEST_SUITE_P(
    GPayPromoCode,
    OfferNotificationBubbleViewsInteractiveUiTest,
    testing::Values(OfferNotificationBubbleViewsInteractiveUiTestData{
        "GPayPromoCode", AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER}));

// TODO(https://crbug.com/1289161): Flaky failures.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Navigation DISABLED_Navigation
#else
#define MAYBE_Navigation Navigation
#endif
IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       MAYBE_Navigation) {
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("http");

  static const struct {
    GURL url_navigated_to;
    bool bubble_should_be_visible;
  } test_cases[] = {
      // Different page on same domain keeps bubble.
      {GetUrl("www.merchantsite1.com", "/second/"), true},
      // Different domain not in offer's list dismisses bubble.
      {GetUrl("www.about.com", "/"), false},
      // Subdomain not in offer's list dismisses bubble.
      {GetUrl("support.merchantsite1.com", "/first/"), false},
      // http vs. https mismatch dismisses bubble.
      {GetUrl("www.merchantsite1.com", "/first/")
           .ReplaceComponents(replace_scheme),
       false},
      // Different domain in the offer's list keeps bubble.
      {GetUrl("www.merchantsite2.com", "/first/"), true},
  };

  // Set the initial origin that the bubble will be displayed on.
  SetUpOfferDataWithDomains(test_offer_type_,
                            {GetUrl("www.merchantsite1.com", "/"),
                             GetUrl("www.merchantsite2.com", "/")});

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StrCat(
        {test_case.url_navigated_to.spec(), ", bubble should be=",
         test_case.bubble_should_be_visible ? "visible" : "invisible"}));
    ClearNotificationActiveDomainsForTesting();
    NavigateTo(GURL(chrome::kChromeUINewTabPageURL));

    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateToAndWaitForForm(GetUrl("www.merchantsite1.com", "/first"));
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
                            {GetUrl("www.merchantsite1.com", "/"),
                             GetUrl("www.merchantsite2.com", "/")});

  // Makes sure the foreground tab is a blank site.
  NavigateTo(GURL("about:blank"));

  // Creates first background tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl("www.merchantsite1.com", "/"),
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
      browser(), GetUrl("www.merchantsite2.com", "/"),
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
  NavigateToAndWaitForForm(GetUrl("www.merchantsite1.com", "/second"));
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  // Navigates to an invalid domain will dismiss the icon.
  NavigateToAndWaitForForm(GetUrl("www.about.com", "/"));
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

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       Logging_LostFocus) {
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
                       TooltipAndAccessibleName) {
  // Applies to free listing coupons offers only, as other offers do not have a
  // clickable promo code copy button.
  if (test_offer_type_ !=
      AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER) {
    return;
  }

  ShowBubbleForOfferAndVerify();
  ASSERT_TRUE(GetOfferNotificationBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  std::u16string normal_button_tooltip = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_NORMAL);
  std::u16string clicked_button_tooltip = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PROMO_CODE_OFFER_BUTTON_TOOLTIP_CLICKED);
  views::LabelButton* copy_promo_code_button;

  if (::features::IsChromeRefresh2023()) {
    copy_promo_code_button =
        GetOfferNotificationBubbleViews()
            ->promo_code_label_view_->GetCopyButtonForTesting();
  } else {
    copy_promo_code_button =
        GetOfferNotificationBubbleViews()->promo_code_label_button_.get();
  }

  EXPECT_EQ(normal_button_tooltip, copy_promo_code_button->GetTooltipText());
  EXPECT_EQ(copy_promo_code_button->GetText() + u" " + normal_button_tooltip,
            copy_promo_code_button->GetAccessibleName());

  GetOfferNotificationBubbleViews()->OnPromoCodeButtonClicked();

  EXPECT_EQ(clicked_button_tooltip, copy_promo_code_button->GetTooltipText());
  EXPECT_EQ(copy_promo_code_button->GetText() + u" " + clicked_button_tooltip,
            copy_promo_code_button->GetAccessibleName());
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       CopyPromoCode) {
  // Applies to free listing coupons offers only, as other offers do not have a
  // clickable promo code copy button.
  if (test_offer_type_ !=
      AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER) {
    return;
  }

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

  views::LabelButton* copy_promo_code_button;

  if (::features::IsChromeRefresh2023()) {
    copy_promo_code_button =
        GetOfferNotificationBubbleViews()
            ->promo_code_label_view_->GetCopyButtonForTesting();
    EXPECT_EQ(copy_promo_code_button->GetText(),
              l10n_util::GetStringUTF16(IDS_DISCOUNT_CODE_COPY_BUTTON_TEXT));
  } else {
    copy_promo_code_button =
        GetOfferNotificationBubbleViews()->promo_code_label_button_.get();
    EXPECT_EQ(copy_promo_code_button->GetText(), test_promo_code);
  }

  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubblePromoCodeButtonClicked." +
          GetSubhistogramNameForOfferType(),
      true, 1);
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
  personal_data()->ClearAllServerData();

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

IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsInteractiveUiTest,
    RecordPageLoadsWithPromoOfferIconShowingMetricForFreeListingOffer) {
  // Applies to free listing coupons offers only, as we don't log this metric
  // for other offers.
  if (test_offer_type_ !=
      AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER) {
    return;
  }

  base::HistogramTester histogram_tester;

  ShowBubbleForOfferAndVerify();
  ASSERT_TRUE(GetOfferNotificationBubbleViews());
  ASSERT_TRUE(IsIconVisible());
  histogram_tester.ExpectBucketCount(
      "Autofill.PageLoadsWithOfferIconShowing.FreeListingCouponOffer", true, 1);

  test_clock_.Advance(kAutofillBubbleSurviveNavigationTime);

  // Navigates to another valid domain will not reshow the bubble.
  NavigateToAndWaitForForm(GetUrl("www.merchantsite1.com", "/second"));
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_TRUE(IsIconVisible());
  histogram_tester.ExpectBucketCount(
      "Autofill.PageLoadsWithOfferIconShowing.FreeListingCouponOffer", true, 2);

  // Navigates to an invalid domain will dismiss the icon.
  NavigateToAndWaitForForm(GetUrl("www.about.com", "/"));
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_FALSE(IsIconVisible());
  histogram_tester.ExpectBucketCount(
      "Autofill.PageLoadsWithOfferIconShowing.FreeListingCouponOffer", true, 2);
}

IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewsInteractiveUiTest,
                       IconViewAccessibleName) {
  EXPECT_EQ(GetOfferNotificationIconView()->GetAccessibleName(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
  EXPECT_EQ(
      GetOfferNotificationIconView()->GetTextForTooltipAndAccessibleName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
}

IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsInteractiveUiTest,
    ShowShoppingServiceFreeListingOffer_WhenGPayPromoCodeOfferNotAvailable) {
  // This test is for when commerce::kShowDiscountOnNavigation is enabled.
  if (!base::FeatureList::IsEnabled(commerce::kShowDiscountOnNavigation)) {
    return;
  }

  const std::string domain_url = "www.merchantsite1.com";
  const GURL with_offer_url = GetUrl(domain_url, "/product1");
  const GURL without_offer_url = GetUrl(domain_url, "/product2");
  const GURL with_merchant_wide_offer_url = GetUrl(domain_url, "/product3");
  const std::string detail = "Discount description detail";
  const std::string discount_code = "freelisting-discount-code";
  const int64_t non_merchant_wide_discount_id = 123;
  const int64_t merchant_wide_discount_id = 456;
  const double expiry_time_sec =
      (AutofillClock::Now() + base::Days(2)).ToDoubleT();

  auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(
          browser()->profile()));
  mock_shopping_service->SetIsDiscountEligibleToShowOnNavigation(true);
  // Expect to call this at least once on every navigation, this test is
  // navigated 4 times.
  EXPECT_CALL(*mock_shopping_service, IsDiscountEligibleToShowOnNavigation)
      .Times(testing::AtLeast(4));
  EXPECT_CALL(*mock_shopping_service, GetDiscountInfoForUrls)
      .Times(testing::AtLeast(4));

  NavigateToAndWaitForForm(GetUrl(domain_url, "/"));
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());

  // Simulate non-merchant-wide FreeListingOffer for a product page on the
  // `with_offer_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            non_merchant_wide_discount_id,
            /*is_merchant_wide=*/false, expiry_time_sec)}}});

  NavigateToAndWaitForForm(with_offer_url);
  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  // Click on the omnibox icon to show the bubble and verify.
  SimulateClickOnIconAndReshowBubble();
  if (::features::IsChromeRefresh2023()) {
    auto* promo_code_label_view =
        GetOfferNotificationBubbleViews()->promo_code_label_view_.get();
    EXPECT_TRUE(promo_code_label_view);
    EXPECT_EQ(base::ASCIIToUTF16(discount_code),
              promo_code_label_view->GetPromoCodeLabelTextForTesting());
  } else {
    auto* promo_code_label_button =
        GetOfferNotificationBubbleViews()->promo_code_label_button_.get();
    EXPECT_TRUE(promo_code_label_button);
    EXPECT_EQ(base::ASCIIToUTF16(discount_code),
              promo_code_label_button->GetText());
  }

  // Navigates to URL without offers will dismiss the icon.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls({});
  NavigateToAndWaitForForm(without_offer_url);
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());

  // Simulate merchant-wide FreeListingOffer for a product page on the
  // `with_merchant_wide_offer_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_merchant_wide_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            merchant_wide_discount_id,
            /*is_merchant_wide=*/true, expiry_time_sec)}}});

  NavigateToAndWaitForForm(with_merchant_wide_offer_url);
  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsInteractiveUiTest,
    ShowShoppingServiceFreeListingOffer_RecordHistoryClusterUsageRelatedMetrics) {
  // This test is for when commerce::kShowDiscountOnNavigation is enabled.
  if (!base::FeatureList::IsEnabled(commerce::kShowDiscountOnNavigation)) {
    return;
  }

  const std::string non_merchant_wide_domain_url = "www.merchantsite1.test";
  const GURL with_non_merchant_wide_offer_url =
      GetUrl(non_merchant_wide_domain_url, "/first");
  const std::string detail = "Discount description detail";
  const std::string discount_code = "freelisting-discount-code";
  const int64_t non_merchant_wide_discount_id = 123;
  const double expiry_time_sec =
      (AutofillClock::Now() + base::Days(2)).ToDoubleT();
  base::HistogramTester histogram_tester;

  auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(
          browser()->profile()));
  mock_shopping_service->SetIsDiscountEligibleToShowOnNavigation(true);
  // Simulate FreeListingOffer for a product page on the
  // `non_merchant_wide_domain_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_non_merchant_wide_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            non_merchant_wide_discount_id,
            /*is_merchant_wide=*/false, expiry_time_sec)}}});

  // Expect to call this at least once on every navigation, this test is
  // navigated 1 time.
  EXPECT_CALL(*mock_shopping_service, IsDiscountEligibleToShowOnNavigation)
      .Times(testing::AtLeast(1));
  EXPECT_CALL(*mock_shopping_service, GetDiscountInfoForUrls)
      .Times(testing::AtLeast(1));

  NavigateToAndWaitForForm(with_non_merchant_wide_offer_url);
  EXPECT_TRUE(IsIconVisible());
  // Without the correct UTM url params, the bubble is expected to not show
  // automatically.
  EXPECT_FALSE(GetOfferNotificationBubbleViews());

  histogram_tester.ExpectBucketCount(
      "Autofill.PageLoadsWithOfferIconShowing.FreeListingCouponOffer."
      "FromHistoryCluster",
      false, 1);

  // Click on the omnibox icon to show the bubble and verify.
  SimulateClickOnIconAndReshowBubble();
  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubbleOffer.FreeListingCouponOffer."
      "FromHistoryCluster",
      false, 1);

  // Simulate clicking on the copy promo code button.
  GetOfferNotificationBubbleViews()->OnPromoCodeButtonClicked();
  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubblePromoCodeButtonClicked."
      "FreeListingCouponOffer.FromHistoryCluster",
      false, 1);
}

IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsInteractiveUiTest,
    ShowGPayPromoCodeOffer_WhenGPayPromoCodeOfferAndShoppingServiceOfferAreBothAvailable) {
  const std::string domain_url = "www.merchantsite1.com";
  const GURL with_offer_url = GetUrl(domain_url, "/first");
  const std::string detail = "Discount description detail";
  const std::string discount_code = "freelisting-discount-code";
  const int64_t discount_id = 123;
  const double expiry_time_sec =
      (AutofillClock::Now() + base::Days(2)).ToDoubleT();

  auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(
          browser()->profile()));
  mock_shopping_service->SetIsDiscountEligibleToShowOnNavigation(true);
  // Simulate FreeListingOffer for a product page on the `domain_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code, discount_id,
            /*is_merchant_wide=*/false, expiry_time_sec)}}});

  EXPECT_CALL(*mock_shopping_service, IsDiscountEligibleToShowOnNavigation)
      .Times(testing::AtLeast(1));
  EXPECT_CALL(*mock_shopping_service, GetDiscountInfoForUrls)
      .Times(testing::AtLeast(1));

  SetUpGPayPromoCodeOfferDataWithDomains(
      {GetUrl("www.merchantsite1.com", "/"),
       GetUrl("www.merchantsite2.com", "/")});
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm(with_offer_url);
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  if (::features::IsChromeRefresh2023()) {
    auto* promo_code_label_view =
        GetOfferNotificationBubbleViews()->promo_code_label_view_.get();
    EXPECT_FALSE(promo_code_label_view);
  } else {
    auto* promo_code_label_button =
        GetOfferNotificationBubbleViews()->promo_code_label_button_.get();
    EXPECT_FALSE(promo_code_label_button);
  }

  auto promo_code_styled_label =
      GetOfferNotificationBubbleViews()->promo_code_label_;
  EXPECT_TRUE(promo_code_styled_label);
  EXPECT_EQ(promo_code_styled_label->GetText(),
            base::ASCIIToUTF16(GetDefaultTestValuePropText()) + u" " +
                base::ASCIIToUTF16(GetDefaultTestSeeDetailsText()));
}

using OfferNotificationBubbleViewsWithDiscountOnChromeHistoryClusterTest =
    OfferNotificationBubbleViewsInteractiveUiTest;

INSTANTIATE_TEST_SUITE_P(
    FreeListingCoupon,
    OfferNotificationBubbleViewsWithDiscountOnChromeHistoryClusterTest,
    testing::Values(
        OfferNotificationBubbleViewsInteractiveUiTestData{
            "FreeListingCoupon_on_history_cluster",
            AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER,
            absl::make_optional<std::vector<base::test::FeatureRefAndParams>>(
                {{ntp_features::kNtpHistoryClustersModuleDiscounts, {}}})},
        OfferNotificationBubbleViewsInteractiveUiTestData{
            "FreeListingCoupon_on_history_cluster_chrome_refresh_style",
            AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER,
            absl::make_optional<std::vector<base::test::FeatureRefAndParams>>(
                {{ntp_features::kNtpHistoryClustersModuleDiscounts, {}},
                 {::features::kChromeRefresh2023, {}}})}),
    GetTestName);

IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsWithDiscountOnChromeHistoryClusterTest,
    ShowShoppingServiceFreeListingOffer_WhenNavigatedFromChromeHistoryCluster) {
  const std::string non_merchant_wide_domain_url = "www.merchantsite1.com";
  const std::string merchant_wide_domain_url = "www.merchantsite2.com";
  const GURL with_non_merchant_wide_offer_url =
      GetUrl(non_merchant_wide_domain_url,
             "/first?utm_source=chrome&utm_medium=app&utm_campaign=chrome-"
             "history-cluster-with-discount");
  const GURL with_merchant_wide_offer_url =
      GetUrl(merchant_wide_domain_url,
             "/first?utm_source=chrome&utm_medium=app&utm_campaign=chrome-"
             "history-cluster-with-discount");
  const std::string detail = "Discount description detail";
  const std::string discount_code = "freelisting-discount-code";
  const int64_t non_merchant_wide_discount_id = 123;
  const int64_t merchant_wide_discount_id = 456;
  const double expiry_time_sec =
      (AutofillClock::Now() + base::Days(2)).ToDoubleT();

  auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(
          browser()->profile()));
  mock_shopping_service->SetIsDiscountEligibleToShowOnNavigation(true);
  // Simulate FreeListingOffer for a product page on the
  // `non_merchant_wide_domain_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_non_merchant_wide_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            non_merchant_wide_discount_id,
            /*is_merchant_wide=*/false, expiry_time_sec)}}});

  // Expect to call this at least once on every navigation, this test is
  // navigated 3 times.
  EXPECT_CALL(*mock_shopping_service, IsDiscountEligibleToShowOnNavigation)
      .Times(testing::AtLeast(3));
  EXPECT_CALL(*mock_shopping_service, GetDiscountInfoForUrls)
      .Times(testing::AtLeast(3));

  SetUpGPayPromoCodeOfferDataWithDomains(
      {GetUrl("www.merchantsite1.com", "/"),
       GetUrl("www.merchantsite2.com", "/")});
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm(with_non_merchant_wide_offer_url);
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  if (::features::IsChromeRefresh2023()) {
    auto* promo_code_label_view =
        GetOfferNotificationBubbleViews()->promo_code_label_view_.get();
    EXPECT_TRUE(promo_code_label_view);
    EXPECT_EQ(base::ASCIIToUTF16(discount_code),
              promo_code_label_view->GetPromoCodeLabelTextForTesting());
  } else {
    auto* promo_code_label_button =
        GetOfferNotificationBubbleViews()->promo_code_label_button_.get();
    EXPECT_TRUE(promo_code_label_button);
    EXPECT_EQ(base::ASCIIToUTF16(discount_code),
              promo_code_label_button->GetText());
  }

  auto promo_code_styled_label =
      GetOfferNotificationBubbleViews()->promo_code_label_;
  EXPECT_FALSE(promo_code_styled_label);

  // Simulate merchant-wide FreeListingOffer for a product page on the
  // `merchant_wide_domain_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_merchant_wide_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            merchant_wide_discount_id,
            /*is_merchant_wide=*/true, expiry_time_sec)}}});

  NavigateToAndWaitForForm(with_merchant_wide_offer_url);
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  // Navigate back to the product page with the non-merchant-wide offer, and
  // verified bubble will not show automatically.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_non_merchant_wide_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            non_merchant_wide_discount_id,
            /*is_merchant_wide=*/false, expiry_time_sec)}}});

  NavigateToAndWaitForForm(with_non_merchant_wide_offer_url);
  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsWithDiscountOnChromeHistoryClusterTest,
    NotShowShoppingServiceFreeListingOfferWithoutUTM) {
  const std::string non_merchant_wide_domain_url = "www.merchantsite1.com";
  const GURL with_non_merchant_wide_offer_url =
      GetUrl(non_merchant_wide_domain_url, "/first");
  const std::string detail = "Discount description detail";
  const std::string discount_code = "freelisting-discount-code";
  const int64_t non_merchant_wide_discount_id = 123;
  const double expiry_time_sec =
      (AutofillClock::Now() + base::Days(2)).ToDoubleT();

  auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(
          browser()->profile()));
  mock_shopping_service->SetIsDiscountEligibleToShowOnNavigation(true);
  // Simulate FreeListingOffer for a product page on the
  // `non_merchant_wide_domain_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_non_merchant_wide_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            non_merchant_wide_discount_id,
            /*is_merchant_wide=*/false, expiry_time_sec)}}});

  // Expect to call this at least once on every navigation, this test is
  // navigated 1 time.
  EXPECT_CALL(*mock_shopping_service, IsDiscountEligibleToShowOnNavigation)
      .Times(testing::AtLeast(1));
  EXPECT_CALL(*mock_shopping_service, GetDiscountInfoForUrls)
      .Times(testing::AtLeast(1));

  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm(with_non_merchant_wide_offer_url);
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

IN_PROC_BROWSER_TEST_P(
    OfferNotificationBubbleViewsWithDiscountOnChromeHistoryClusterTest,
    RecordHistoryClusterUsageRelatedMetrics) {
  const std::string non_merchant_wide_domain_url = "www.merchantsite1.test";
  const GURL with_non_merchant_wide_offer_url =
      GetUrl(non_merchant_wide_domain_url,
             "/first?utm_source=chrome&utm_medium=app&utm_campaign=chrome-"
             "history-cluster-with-discount");
  const std::string detail = "Discount description detail";
  const std::string discount_code = "freelisting-discount-code";
  const int64_t non_merchant_wide_discount_id = 123;
  const double expiry_time_sec =
      (AutofillClock::Now() + base::Days(2)).ToDoubleT();
  base::HistogramTester histogram_tester;

  auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
      commerce::ShoppingServiceFactory::GetForBrowserContext(
          browser()->profile()));
  mock_shopping_service->SetIsDiscountEligibleToShowOnNavigation(true);
  // Simulate FreeListingOffer for a product page on the
  // `non_merchant_wide_domain_url`.
  mock_shopping_service->SetResponseForGetDiscountInfoForUrls(
      {{with_non_merchant_wide_offer_url,
        {commerce::CreateValidDiscountInfo(
            detail, /*terms_and_conditions=*/"",
            /*value_in_text=*/"$10 off", discount_code,
            non_merchant_wide_discount_id,
            /*is_merchant_wide=*/false, expiry_time_sec)}}});

  // Expect to call this at least once on every navigation, this test is
  // navigated 1 time.
  EXPECT_CALL(*mock_shopping_service, IsDiscountEligibleToShowOnNavigation)
      .Times(testing::AtLeast(1));
  EXPECT_CALL(*mock_shopping_service, GetDiscountInfoForUrls)
      .Times(testing::AtLeast(1));

  NavigateToAndWaitForForm(with_non_merchant_wide_offer_url);
  EXPECT_TRUE(IsIconVisible());
  // With the correct URM url params, the bubble is expected to show
  // automatically.
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  histogram_tester.ExpectBucketCount(
      "Autofill.PageLoadsWithOfferIconShowing.FreeListingCouponOffer."
      "FromHistoryCluster",
      true, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubbleOffer.FreeListingCouponOffer."
      "FromHistoryCluster",
      true, 1);

  // Simulate clicking on the copy promo code button.
  GetOfferNotificationBubbleViews()->OnPromoCodeButtonClicked();
  histogram_tester.ExpectBucketCount(
      "Autofill.OfferNotificationBubblePromoCodeButtonClicked."
      "FreeListingCouponOffer.FromHistoryCluster",
      true, 1);
}

}  // namespace autofill
