// Copyright 2021 The Chromium Authors
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
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class OfferNotificationBubbleControllerImplTest
    : public BrowserWithTestWindowTest {
 public:
  OfferNotificationBubbleControllerImplTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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
    MOCK_METHOD(void,
                RecordCouponDisplayTimestamp,
                (const autofill::AutofillOfferData& offer));
    MOCK_METHOD(base::Time,
                GetCouponDisplayTimestamp,
                (const autofill::AutofillOfferData& offer));
  };

  void ShowBubble(const AutofillOfferData& offer,
                  bool expand_notification_icon = false) {
    controller()->ShowOfferNotificationIfApplicable(
        offer, &card_,
        {.expand_notification_icon = expand_notification_icon,
         .show_notification_automatically = true});
  }

  void CloseBubble(PaymentsBubbleClosedReason closed_reason =
                       PaymentsBubbleClosedReason::kNotInteracted) {
    controller()->OnBubbleClosed(closed_reason);
  }

  void CloseAndReshowBubble() {
    CloseBubble();
    controller()->ReshowBubble();
  }

  AutofillOfferData CreateTestCardLinkedOffer(
      const std::vector<GURL>& merchant_origins,
      const std::vector<int64_t>& eligible_instrument_ids = {}) {
    int64_t offer_id = 1357;
    base::Time expiry = base::Time::Now() + base::Days(2);
    GURL offer_details_url("https://www.google.com/");
    std::string offer_reward_amount = "5%";
    return autofill::AutofillOfferData::GPayCardLinkedOffer(
        offer_id, expiry, merchant_origins, offer_details_url,
        autofill::DisplayStrings(), eligible_instrument_ids,
        offer_reward_amount);
  }

  AutofillOfferData CreateTestGPayPromoCodeOffer(
      const std::vector<GURL>& merchant_origins,
      const std::string& promo_code) {
    int64_t offer_id = 2468;
    base::Time expiry = base::Time::Now() + base::Days(2);
    autofill::DisplayStrings display_strings;
    display_strings.value_prop_text = "5% off on shoes. Up to $50.";
    display_strings.see_details_text = "See details";
    display_strings.usage_instructions_text =
        "Click the promo code field at checkout to autofill it.";
    GURL offer_details_url = GURL("https://pay.google.com");
    return autofill::AutofillOfferData::GPayPromoCodeOffer(
        offer_id, expiry, merchant_origins, offer_details_url, display_strings,
        promo_code);
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

  MockCouponService mock_coupon_service_;

 private:
  CreditCard card_ = test::GetCreditCard();
};

TEST_F(OfferNotificationBubbleControllerImplTest, BubbleShown) {
  // Check that bubble is visible.
  AutofillOfferData offer = CreateTestCardLinkedOffer(
      /*merchant_origins=*/{GURL("https://www.example.com")},
      /*eligible_instrument_ids=*/{123});
  ShowBubble(offer);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
}

// Ensures the bubble does not stick around after it has been shown for longer
// than kAutofillBubbleSurviveNavigationTime (5 seconds).
TEST_F(OfferNotificationBubbleControllerImplTest,
       OfferBubbleDismissesOnNavigation) {
  AutofillOfferData offer = CreateTestCardLinkedOffer(
      /*merchant_origins=*/{GURL("https://www.example.com")},
      /*eligible_instrument_ids=*/{123});
  ShowBubble(offer);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
  task_environment()->FastForwardBy(kAutofillBubbleSurviveNavigationTime -
                                    base::Seconds(1));
  controller()->ShowOfferNotificationIfApplicable(
      offer, nullptr, {.notification_has_been_shown = true});
  // Ensure the bubble is still there if
  // kOfferNotificationBubbleSurviveNavigationTime hasn't been reached yet.
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

  task_environment()->FastForwardBy(base::Seconds(2));
  controller()->ShowOfferNotificationIfApplicable(
      offer, nullptr, {.notification_has_been_shown = true});
  // Ensure new page does not have an active offer notification bubble.
  EXPECT_EQ(nullptr, controller()->GetOfferNotificationBubbleView());
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       ShownOfferIsRetrievableFromController) {
  AutofillOfferData offer = CreateTestCardLinkedOffer(
      /*merchant_origins=*/{GURL("https://www.example.com")},
      /*eligible_instrument_ids=*/{123});
  ShowBubble(offer);

  EXPECT_TRUE(*controller()->GetOffer() == offer);
}

// Tests that the offer notification bubble will be shown, and coupon service
// will not be called for a GPay promo code offer.
TEST_F(OfferNotificationBubbleControllerImplTest, GPayPromoCode_BubbleShown) {
  AutofillOfferData offer = CreateTestGPayPromoCodeOffer(
      /*merchant_origins=*/{GURL("https://www.example.com")},
      /*promo_code=*/"FREEFALL5678");
  ShowBubble(offer);

  EXPECT_CALL(mock_coupon_service_, GetCouponDisplayTimestamp).Times(0);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_GPAY_PROMO_CODE_OFFERS_REMINDER_TITLE));
}

// Tests that the offer notification icon will be expanded.
TEST_F(OfferNotificationBubbleControllerImplTest,
       OfferNotificationIconShouldBeExpanded) {
  AutofillOfferData offer = CreateTestGPayPromoCodeOffer(
      /*merchant_origins=*/{GURL("https://www.example.com")},
      /*promo_code=*/"FREEFALL1234");
  controller()->ShowOfferNotificationIfApplicable(
      offer, nullptr,
      {.notification_has_been_shown = true, .expand_notification_icon = true});

  EXPECT_TRUE(controller()->ShouldIconExpand());
}

}  // namespace autofill
