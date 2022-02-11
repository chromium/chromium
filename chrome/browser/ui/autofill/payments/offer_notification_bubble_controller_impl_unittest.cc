// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
class TestOfferNotificationBubbleControllerImpl
    : public OfferNotificationBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestOfferNotificationBubbleControllerImpl>(
            web_contents));
  }

  explicit TestOfferNotificationBubbleControllerImpl(
      content::WebContents* web_contents)
      : OfferNotificationBubbleControllerImpl(web_contents) {}

 private:
  // Overrides to bypass the IsWebContentsActive check.
  bool IsWebContentsActive() override { return true; }
};

}  // namespace

class OfferNotificationBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  OfferNotificationBubbleControllerImplTest() = default;
  OfferNotificationBubbleControllerImplTest(
      const OfferNotificationBubbleControllerImplTest&) = delete;
  OfferNotificationBubbleControllerImplTest& operator=(
      const OfferNotificationBubbleControllerImplTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(GURL("about:blank"));
    TestOfferNotificationBubbleControllerImpl::CreateForTesting(web_contents());
    static_cast<TestOfferNotificationBubbleControllerImpl*>(
        TestOfferNotificationBubbleControllerImpl::FromWebContents(
            web_contents()))
        ->coupon_service_ = &mock_coupon_service_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  virtual void AddTab(const GURL& url) {
    BrowserWithTestWindowTest::AddTab(browser(), url);
  }

 protected:
  class MockCouponService : public CouponService {
   public:
    MOCK_METHOD1(RecordCouponDisplayTimestamp,
                 void(const autofill::AutofillOfferData& offer));
    MOCK_METHOD1(GetCouponDisplayTimestamp,
                 base::Time(const autofill::AutofillOfferData& offer));
  };

  void ShowBubble(const AutofillOfferData* offer) {
    controller()->ShowOfferNotificationIfApplicable(
        offer, &card_, /*should_show_icon_only=*/false);
  }

  void CloseBubble(PaymentsBubbleClosedReason closed_reason =
                       PaymentsBubbleClosedReason::kNotInteracted) {
    controller()->OnBubbleClosed(closed_reason);
  }

  void CloseAndReshowBubble() {
    CloseBubble();
    controller()->ReshowBubble();
  }

  AutofillOfferData CreateTestOfferWithOrigins(
      const std::vector<GURL>& merchant_origins) {
    // Only adding what the tests need to pass. Feel free to add more populated
    // fields as necessary.
    AutofillOfferData offer;
    offer.offer_id = 1357;
    offer.merchant_origins = merchant_origins;
    return offer;
  }

  TestOfferNotificationBubbleControllerImpl* controller() {
    return static_cast<TestOfferNotificationBubbleControllerImpl*>(
        TestOfferNotificationBubbleControllerImpl::FromWebContents(
            web_contents()));
  }

  void SetCouponServiceForController(
      TestOfferNotificationBubbleControllerImpl* controller,
      CouponService* coupon_service) {
    controller->coupon_service_ = coupon_service;
  }

  TestAutofillClock test_clock_;
  MockCouponService mock_coupon_service_;

 private:
  CreditCard card_ = test::GetCreditCard();
};

TEST_F(OfferNotificationBubbleControllerImplTest, BubbleShown) {
  // Check that bubble is visible.
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
  offer.eligible_instrument_id = {123};
  ShowBubble(&offer);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
}

// Ensures the bubble does not stick around after it has been shown for longer
// than kAutofillBubbleSurviveNavigationTime (5 seconds).
TEST_F(OfferNotificationBubbleControllerImplTest,
       OfferBubbleDismissesOnNavigation) {
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
  offer.eligible_instrument_id = {123};
  ShowBubble(&offer);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
  test_clock_.Advance(kAutofillBubbleSurviveNavigationTime - base::Seconds(1));
  controller()->ShowOfferNotificationIfApplicable(
      &offer, nullptr, /*should_show_icon_only=*/true);
  // Ensure the bubble is still there if
  // kOfferNotificationBubbleSurviveNavigationTime hasn't been reached yet.
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

  test_clock_.Advance(base::Seconds(2));
  controller()->ShowOfferNotificationIfApplicable(
      &offer, nullptr, /*should_show_icon_only=*/true);
  // Ensure new page does not have an active offer notification bubble.
  EXPECT_EQ(nullptr, controller()->GetOfferNotificationBubbleView());
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       ShownOfferIsRetrievableFromController) {
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
  offer.eligible_instrument_id = {123};
  ShowBubble(&offer);

  EXPECT_EQ(&offer, controller()->GetOffer());
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       FreeListing_NotShownWithinTimeGap) {
  base::HistogramTester histogram_tester;
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
  offer.promo_code = "FREEFALL1234";
  // Try to show a FreeListing coupon whose last shown timestamp is within time
  // gap.
  EXPECT_CALL(mock_coupon_service_, GetCouponDisplayTimestamp(offer))
      .Times(1)
      .WillOnce(::testing::Return(base::Time::Now()));
  EXPECT_CALL(mock_coupon_service_, RecordCouponDisplayTimestamp(offer))
      .Times(0);

  ShowBubble(&offer);

  histogram_tester.ExpectTotalCount(
      "Autofill.OfferNotificationBubbleSuppressed.FreeListingCouponOffer", 1);
  EXPECT_FALSE(controller()->GetOfferNotificationBubbleView());
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       FreeListing_ShownBeyondTimeGap) {
  base::HistogramTester histogram_tester;
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
  offer.promo_code = "FREEFALL1234";
  // Try to show a FreeListing coupon whose last shown timestamp is beyond time
  // gap.
  EXPECT_CALL(mock_coupon_service_, GetCouponDisplayTimestamp(offer))
      .Times(1)
      .WillOnce(::testing::Return(base::Time::Now() -
                                  commerce::kCouponDisplayInterval.Get() -
                                  base::Seconds(1)));
  EXPECT_CALL(mock_coupon_service_, RecordCouponDisplayTimestamp(offer))
      .Times(1);

  ShowBubble(&offer);

  histogram_tester.ExpectTotalCount(
      "Autofill.OfferNotificationBubbleSuppressed.FreeListingCouponOffer", 0);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       FreeListing_OnCouponInvalidated) {
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
  offer.promo_code = "FREEFALL1234";
  EXPECT_CALL(mock_coupon_service_, GetCouponDisplayTimestamp(offer))
      .Times(1)
      .WillOnce(::testing::Return(base::Time::Now() -
                                  commerce::kCouponDisplayInterval.Get() -
                                  base::Seconds(1)));
  ShowBubble(&offer);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

  AutofillOfferData offer2 = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
  offer2.promo_code = "FREEFALL5678";
  controller()->OnCouponInvalidated(offer2);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

  controller()->OnCouponInvalidated(offer);
  EXPECT_FALSE(controller()->GetOfferNotificationBubbleView());
}

}  // namespace autofill
