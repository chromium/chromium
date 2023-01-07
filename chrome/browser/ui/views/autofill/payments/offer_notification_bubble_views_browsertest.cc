// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class OfferNotificationBubbleViewsBrowserTest
    : public OfferNotificationBubbleViewsTestBase {
 public:
  OfferNotificationBubbleViewsBrowserTest() = default;
  ~OfferNotificationBubbleViewsBrowserTest() override = default;
  OfferNotificationBubbleViewsBrowserTest(
      const OfferNotificationBubbleViewsBrowserTest&) = delete;
  OfferNotificationBubbleViewsBrowserTest& operator=(
      const OfferNotificationBubbleViewsBrowserTest&) = delete;
};

// Tests that the offer notification bubble will not be shown if the offer data
// is invalid (does not have a linked card or a promo code).
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       InvalidOfferData) {
  auto offer_data = CreateCardLinkedOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
  offer_data->SetEligibleInstrumentIdForTesting({});
  personal_data()->AddOfferDataForTest(std::move(offer_data));
  personal_data()->NotifyPersonalDataObserver();

  // Neither icon nor bubble should be visible.
  NavigateToAndWaitForForm("https://www.example.com/first/");
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

// TODO(crbug.com/1270516): Does not work for Wayland-based tests.
// TODO(crbug.com/1256480): Disabled on Mac, Win, ChromeOS, and Lacros due to
// flakiness.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       DISABLED_PromoCodeOffer) {
  auto offer_data = CreateGPayPromoCodeOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
  personal_data()->AddOfferDataForTest(std::move(offer_data));
  personal_data()->NotifyPersonalDataObserver();

  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm("https://www.example.com/first/");
  WaitForObservedEvent();

  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());
}

// TODO(crbug.com/1256480): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       DISABLED_PromoCodeOffer_FromCouponService) {
  auto offer_data = CreateFreeListingCouponDataWithDomains(
      {GURL("https://www.example.com/")});
  SetUpFreeListingCouponOfferDataForCouponService(std::move(offer_data));

  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm("https://www.example.com/first/");
  WaitForObservedEvent();

  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());
}

IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       PromoCodeOffer_FromCouponService_WithinTimeGap) {
  const GURL orgin("https://www.example.com/");
  SetUpFreeListingCouponOfferDataForCouponService(
      CreateFreeListingCouponDataWithDomains({orgin}));
  UpdateFreeListingCouponDisplayTime(
      CreateFreeListingCouponDataWithDomains({orgin}));

  NavigateToAndWaitForForm("https://www.example.com/first/");

  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

// TODO(crbug.com/1270516): Disabled due to flakiness with linux-wayland-rel.
// Tests that the offer notification bubble will not be shown if bubble has been
// shown for kAutofillBubbleSurviveNavigationTime (5 seconds) and the user has
// opened another tab on the same website.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       DISABLED_BubbleNotShowingOnDuplicateTab) {
  SetUpCardLinkedOfferDataWithDomains({GURL("https://www.example.com/")});

  TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());
  NavigateToAndWaitForForm("https://www.example.com/first/");
  test_clock.Advance(kAutofillBubbleSurviveNavigationTime - base::Seconds(1));
  NavigateToAndWaitForForm("https://www.example.com/second/");
  // Ensure the bubble is still there if
  // kOfferNotificationBubbleSurviveNavigationTime hasn't been reached yet.
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  test_clock.Advance(base::Seconds(2));
  NavigateToAndWaitForForm("https://www.example.com/second/");
  // As kAutofillBubbleSurviveNavigationTime has been reached, the bubble should
  // no longer be showing.
  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

// TODO(crbug.com/1256480): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       DISABLED_PromoCodeOffer_DeleteCoupon) {
  auto offer_data = CreateFreeListingCouponDataWithDomains(
      {GURL("https://www.example.com/")});
  SetUpFreeListingCouponOfferDataForCouponService(std::move(offer_data));

  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm("https://www.example.com/first/");
  WaitForObservedEvent();

  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  DeleteFreeListingCouponForUrl(GURL("https://www.example.com/"));

  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

class OfferNotificationBubbleViewsBrowserTestWithoutPromoCodes
    : public OfferNotificationBubbleViewsTestBase {
 public:
  OfferNotificationBubbleViewsBrowserTestWithoutPromoCodes()
      : OfferNotificationBubbleViewsTestBase(
            /*promo_code_flag_enabled=*/false) {}
  ~OfferNotificationBubbleViewsBrowserTestWithoutPromoCodes() override =
      default;
  OfferNotificationBubbleViewsBrowserTestWithoutPromoCodes(
      const OfferNotificationBubbleViewsBrowserTestWithoutPromoCodes&) = delete;
  OfferNotificationBubbleViewsBrowserTest& operator=(
      const OfferNotificationBubbleViewsBrowserTestWithoutPromoCodes&) = delete;
};

// Tests that the offer notification bubble will not be shown for a promo code
// offer if the feature flag is disabled.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTestWithoutPromoCodes,
                       NoPromoCodeOffer) {
  auto offer_data = CreateGPayPromoCodeOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
  personal_data()->AddOfferDataForTest(std::move(offer_data));
  personal_data()->NotifyPersonalDataObserver();

  // Neither icon nor bubble should be visible.
  NavigateTo("https://www.example.com/first/");
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

}  // namespace autofill
