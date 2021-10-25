// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/commerce/commerce_feature_list.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "content/public/test/mock_navigation_handle.h"
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
  void DoShowBubble() override {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    set_bubble_view(
        browser->window()
            ->GetAutofillBubbleHandler()
            ->ShowOfferNotificationBubble(web_contents(), this,
                                          /*is_user_gesture=*/false));
    DCHECK(bubble_view());
  }
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
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestOfferNotificationBubbleControllerImpl::CreateForTesting(web_contents);
    static_cast<TestOfferNotificationBubbleControllerImpl*>(
        TestOfferNotificationBubbleControllerImpl::FromWebContents(
            web_contents))
        ->coupon_service_ = &mock_coupon_service_;
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
    controller()->ShowOfferNotificationIfApplicable(offer, &card_);
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
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

  MockCouponService mock_coupon_service_;

 private:
  CreditCard card_ = test::GetCreditCard();
};

TEST_F(OfferNotificationBubbleControllerImplTest, BubbleShown) {
  // Check that bubble is visible.
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").GetOrigin()});
  ShowBubble(&offer);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
}

TEST_F(OfferNotificationBubbleControllerImplTest, OriginSticky) {
  static const struct {
    GURL url_navigated_to;
    bool bubble_should_be_visible;
  } test_cases[] = {
      {GURL("https://www.example.com/second/"), true},
      {GURL("https://www.about.com/"), false},
      {GURL("https://support.example.com/first/"), false},
      {GURL("http://www.example.com/first/"), false},
      {GURL("https://www.test.com/first/"), true},
  };

  for (const auto& test_case : test_cases) {
    // Set the initial origin that the bubble will be displayed on.
    NavigateAndCommitActiveTab(GURL("https://www.example.com/first/"));
    AutofillOfferData offer = CreateTestOfferWithOrigins(
        {GURL("https://www.example.com/first/").GetOrigin(),
         GURL("https://www.test.com/first/").GetOrigin()});
    ShowBubble(&offer);

    // Bubble should be visible.
    EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

    // Navigate to a different url, and verify bubble visibility.
    NavigateAndCommitActiveTab(test_case.url_navigated_to);
    EXPECT_EQ(test_case.bubble_should_be_visible,
              !!controller()->GetOfferNotificationBubbleView());
  }
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       ShownOfferIsRetrievableFromController) {
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").GetOrigin()});
  ShowBubble(&offer);

  EXPECT_EQ(&offer, controller()->GetOffer());
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       FreeListing_NotShownWithinTimeGap) {
  base::HistogramTester histogram_tester;
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").GetOrigin()});
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
      {GURL("https://www.example.com/first/").GetOrigin()});
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

}  // namespace autofill
