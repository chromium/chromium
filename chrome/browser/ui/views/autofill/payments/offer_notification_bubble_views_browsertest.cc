// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
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
      {GetUrl("www.example.com", "/"), GetUrl("www.test.com", "/")});
  offer_data->SetEligibleInstrumentIdForTesting({});
  test_api(personal_data()->payments_data_manager())
      .AddOfferData(std::move(offer_data));
  personal_data()->NotifyPersonalDataObserver();

  // Neither icon nor bubble should be visible.
  NavigateToAndWaitForForm(GetUrl("www.example.com", "/first"));
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

// TODO(crbug.com/40205397): Does not work for Wayland-based tests.
// TODO(crbug.com/40200304): Disabled on Mac, Win, ChromeOS, and Lacros due to
// flakiness.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       DISABLED_PromoCodeOffer) {
  auto offer_data = CreateGPayPromoCodeOfferDataWithDomains(
      {GetUrl("www.example.com", "/"), GetUrl("www.test.com", "/")});
  test_api(personal_data()->payments_data_manager())
      .AddOfferData(std::move(offer_data));
  personal_data()->NotifyPersonalDataObserver();

  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateToAndWaitForForm(GetUrl("www.example.com", "/first"));
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());
}

// TODO(crbug.com/40205397): Disabled due to flakiness with linux-wayland-rel.
// Tests that the offer notification bubble will not be shown if bubble has been
// shown for kAutofillBubbleSurviveNavigationTime (5 seconds) and the user has
// opened another tab on the same website.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       DISABLED_BubbleNotShowingOnDuplicateTab) {
  SetUpCardLinkedOfferDataWithDomains({GetUrl("www.example.com", "/")});

  TestAutofillClock test_clock;
  test_clock.SetNow(base::Time::Now());
  NavigateToAndWaitForForm(GetUrl("www.example.com", "/first"));
  test_clock.Advance(kAutofillBubbleSurviveNavigationTime - base::Seconds(1));
  NavigateToAndWaitForForm(GetUrl("www.example.com", "/second"));
  // Ensure the bubble is still there if
  // kOfferNotificationBubbleSurviveNavigationTime hasn't been reached yet.
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  test_clock.Advance(base::Seconds(2));
  NavigateToAndWaitForForm(GetUrl("www.example.com", "/second"));
  // As kAutofillBubbleSurviveNavigationTime has been reached, the bubble should
  // no longer be showing.
  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

}  // namespace autofill
