// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "chrome/browser/ui/autofill/mock_bubble_manager.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

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

  void UpdatePageActionIcon() override {}

 private:
  // Overrides to bypass the IsWebContentsActive check.
  bool IsWebContentsActive() override { return true; }
};

}  // namespace

// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.
class OfferNotificationBubbleControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  OfferNotificationBubbleControllerImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  OfferNotificationBubbleControllerImplTest(
      const OfferNotificationBubbleControllerImplTest&) = delete;
  OfferNotificationBubbleControllerImplTest& operator=(
      const OfferNotificationBubbleControllerImplTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Ensure WebContents is navigated and process is fully initialized.
    NavigateAndCommit(GURL("about:blank"));

    // Create MockTabInterface.
    auto mock_tab = std::make_unique<tabs::MockTabInterface>();

    // Configure MockTabInterface to return our web_contents() when
    // GetContents() is called.
    ON_CALL(*mock_tab, GetContents())
        .WillByDefault(testing::Return(web_contents()));

    // Create TabFeatures.
    auto tab_features = std::make_unique<tabs::TabFeatures>();

    // Create MockBubbleManager.
    auto mock_bubble_manager =
        std::make_unique<testing::NiceMock<MockBubbleManager>>();

    // Configure MockBubbleManager to immediately show bubble when requested.
    ON_CALL(*mock_bubble_manager, RequestShowController(testing::_, testing::_))
        .WillByDefault(
            [](BubbleControllerBase& controller_to_show, bool force_show) {
              controller_to_show.ShowBubble();
            });

    // Configure MockBubbleManager to return false for
    // HasConflictingPendingBubble.
    ON_CALL(*mock_bubble_manager, HasConflictingPendingBubble(testing::_))
        .WillByDefault(testing::Return(false));

    tab_features->SetBubbleManagerForTesting(std::move(mock_bubble_manager));

    // Configure MockTabInterface to return our TabFeatures.
    ON_CALL(*mock_tab, GetTabFeatures())
        .WillByDefault(testing::Return(tab_features.get()));
    ON_CALL(testing::Const(*mock_tab), GetTabFeatures())
        .WillByDefault(testing::Return(tab_features.get()));

    // Configure MockBrowserWindowInterface to return our UnownedUserDataHost.
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    // Configure MockTabInterface to return our MockBrowserWindowInterface.
    ON_CALL(*mock_tab, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&mock_browser_window_interface_));

    // Bind TestAutofillBubbleHandler to the UnownedUserDataHost.
    scoped_autofill_bubble_handler_ = std::make_unique<
        ui::ScopedUnownedUserData<autofill::AutofillBubbleHandler>>(
        unowned_user_data_host_, test_autofill_bubble_handler_);

    // Keep TabFeatures and MockTabInterface alive by storing them in the test
    // fixture.
    tab_features_ = std::move(tab_features);
    tab_interface_ = std::move(mock_tab);

    // Associate the mock TabInterface with the WebContents.
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         tab_interface_.get());

    TestOfferNotificationBubbleControllerImpl::CreateForTesting(web_contents());
  }

  TestOfferNotificationBubbleControllerImpl* controller() {
    return static_cast<TestOfferNotificationBubbleControllerImpl*>(
        TestOfferNotificationBubbleControllerImpl::FromWebContents(
            web_contents()));
  }

 protected:
  void ShowBubble(const AutofillOfferData& offer) {
    controller()->ShowOfferNotificationIfApplicable(
        offer, &card_, {.show_notification_automatically = true});
  }

  void CloseBubble(PaymentsUiClosedReason closed_reason =
                       PaymentsUiClosedReason::kNotInteracted) {
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
    return AutofillOfferData::GPayCardLinkedOffer(
        offer_id, expiry, merchant_origins, offer_details_url, DisplayStrings(),
        eligible_instrument_ids, offer_reward_amount);
  }

  AutofillOfferData CreateTestGPayPromoCodeOffer(
      const std::vector<GURL>& merchant_origins,
      const std::string& promo_code) {
    int64_t offer_id = 2468;
    base::Time expiry = base::Time::Now() + base::Days(2);
    DisplayStrings display_strings;
    display_strings.value_prop_text = "5% off on shoes. Up to $50.";
    display_strings.see_details_text = "See details";
    display_strings.usage_instructions_text =
        "Click the promo code field at checkout to autofill it.";
    GURL offer_details_url = GURL("https://pay.google.com");
    return AutofillOfferData::GPayPromoCodeOffer(
        offer_id, expiry, merchant_origins, offer_details_url, display_strings,
        promo_code);
  }

 private:
  CreditCard card_ = test::GetCreditCard();
  std::unique_ptr<tabs::TabInterface> tab_interface_;
  std::unique_ptr<tabs::TabFeatures> tab_features_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  autofill::TestAutofillBubbleHandler test_autofill_bubble_handler_;
  std::unique_ptr<ui::ScopedUnownedUserData<autofill::AutofillBubbleHandler>>
      scoped_autofill_bubble_handler_;
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

  EXPECT_TRUE(controller()->GetOfferNotificationBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_GPAY_PROMO_CODE_OFFERS_REMINDER_TITLE));
}

}  // namespace autofill
