// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/commerce/commerce_feature_list.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

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
        {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL(),
         GURL("https://www.test.com/first/").DeprecatedGetOriginAsURL()});
    ShowBubble(&offer);

    // Bubble should be visible.
    EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

    // Navigate to a different url, and verify bubble visibility.
    NavigateAndCommitActiveTab(test_case.url_navigated_to);
    EXPECT_EQ(test_case.bubble_should_be_visible,
              !!controller()->GetOfferNotificationBubbleView());
  }
}

// Ensures the bubble does not stick around after it has been shown for longer
// than kAutofillBubbleSurviveNavigationTime (5 seconds).
TEST_F(OfferNotificationBubbleControllerImplTest,
       OfferBubbleDismissesOnNavigation) {
  const GURL& original_url = GURL("https://www.example.com/first/");
  const GURL& second_url = GURL("https://www.example.com/second/");
  NavigateAndCommitActiveTab(original_url);

  // Ensure a bubble is visible on the primary page.
  AutofillOfferData offer =
      CreateTestOfferWithOrigins({original_url.DeprecatedGetOriginAsURL()});
  ShowBubble(&offer);
  test_clock_.Advance(kAutofillBubbleSurviveNavigationTime - base::Seconds(1));
  NavigateAndCommitActiveTab(second_url);
  // Ensure the bubble is still there if
  // kOfferNotificationBubbleSurviveNavigationTime hasn't been reached yet.
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

  test_clock_.Advance(base::Seconds(2));
  NavigateAndCommitActiveTab(original_url);
  // Ensure new page does not have an active offer notification bubble.
  EXPECT_EQ(nullptr, controller()->GetOfferNotificationBubbleView());
}

TEST_F(OfferNotificationBubbleControllerImplTest,
       ShownOfferIsRetrievableFromController) {
  AutofillOfferData offer = CreateTestOfferWithOrigins(
      {GURL("https://www.example.com/first/").DeprecatedGetOriginAsURL()});
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

class OfferNotificationBubbleControllerImplPrerenderTest
    : public OfferNotificationBubbleControllerImplTest {
 public:
  OfferNotificationBubbleControllerImplPrerenderTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // This feature is to run test on any bot.
        {blink::features::kPrerender2MemoryControls});
  }

  // BrowserWithTestWindowTest::AddTab creates a real WebContentsImpl object,
  // but we need a TestWebContents object to unit test prerendering, so we
  // override AddTab to create a TestWebContents instead.
  void AddTab(const GURL& url) override {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
    EXPECT_EQ(web_contents(), raw_contents);
    content::WebContentsTester::For(raw_contents)->NavigateAndCommit(url);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OfferNotificationBubbleControllerImplPrerenderTest,
       DoNotHideBubbleForPrerender) {
  const GURL& original_url = GURL("https://www.example.com/first/");
  NavigateAndCommitActiveTab(original_url);

  // Ensure a bubble is visible on the primary page.
  AutofillOfferData offer =
      CreateTestOfferWithOrigins({original_url.DeprecatedGetOriginAsURL()});
  ShowBubble(&offer);
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

  // Start a prerender and navigate the test page.
  const GURL& prerender_url = GURL("https://www.example.com/second/");
  content::RenderFrameHost* prerender_frame =
      content::WebContentsTester::For(web_contents())
          ->AddPrerenderAndCommitNavigation(prerender_url);
  ASSERT_EQ(prerender_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  // Ensure a bubble is still visible on the primary page.
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());

  // Activate the prerendered page.
  content::NavigationSimulator::CreateRendererInitiated(
      prerender_url, web_contents()->GetMainFrame())
      ->Commit();

  // Ensure a bubble is still visible on the activated page since it has the
  // same-origin as the original url. Cross-origin prerendering is unsupported
  // right now, so this navigation will always be same-origin.
  // TODO(crbug.com/1176054): Add a cross-origin test when prerendering
  // eventually support it.
  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
}

}  // namespace autofill
