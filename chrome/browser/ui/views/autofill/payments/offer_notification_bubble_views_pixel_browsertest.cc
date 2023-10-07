// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/commerce/core/commerce_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/any_widget_observer.h"

#include "chrome/test/base/test_browser_window.h"
namespace autofill {

namespace {
const char kTestURL[] = "https://www.example.com/first/";
const char kTestPromoCode[] = "FREEFALL1234";

AutofillOfferData CreateTestFreeListingCouponOffer(
    const std::vector<GURL>& merchant_origins,
    const std::string& promo_code) {
  int64_t offer_id = 2468;
  base::Time expiry = base::Time::Now() + base::Days(2);
  autofill::DisplayStrings display_strings;
  display_strings.value_prop_text = "5% off on shoes";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";
  return autofill::AutofillOfferData::FreeListingCouponOffer(
      offer_id, expiry, merchant_origins, /*offer_details_url=*/GURL(),
      display_strings, promo_code);
}
}  // namespace

struct OfferNotificationBubbleViewPixelTestConfig {
  std::string name;
  absl::optional<std::vector<base::test::FeatureRefAndParams>> enabled_features;
};

std::string GetTestName(
    const ::testing::TestParamInfo<OfferNotificationBubbleViewPixelTestConfig>&
        info) {
  return info.param.name;
}

// Pixel test for OfferNotificationBubbleView. Pixel tests run in
// DialogBrowserTest::VerifyUi().
class OfferNotificationBubbleViewPixelBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<
          OfferNotificationBubbleViewPixelTestConfig> {
 public:
  OfferNotificationBubbleViewPixelBrowserTest() {
    if (GetParam().enabled_features.has_value()) {
      feature_list_.InitWithFeaturesAndParameters(
          GetParam().enabled_features.value(),
          /*disabled_features=*/{});
    }
  }
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    AutofillOfferData offer = CreateTestFreeListingCouponOffer(
        /*merchant_origins=*/{GURL(kTestURL).DeprecatedGetOriginAsURL()},
        kTestPromoCode);
    auto* autofill_client =
        ChromeAutofillClient::FromWebContentsForTesting(GetWebContents());

    auto offer_notification_bubble_view_waiter = views::NamedWidgetShownWaiter(
        views::test::AnyWidgetTestPasskey{},
        OfferNotificationBubbleViews::kViewClassName);

    autofill_client->UpdateOfferNotification(&offer, {});
    OfferNotificationBubbleControllerImpl* controller = GetController();
    EXPECT_TRUE(controller);
    // Ensure the window is active before reshowing the bubble.
    static_cast<TestBrowserWindow*>(browser()->window())->set_is_active(true);
    controller->ReshowBubble();
    EXPECT_TRUE(offer_notification_bubble_view_waiter.WaitIfNeededAndGet());
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  OfferNotificationBubbleViews* GetOfferNotificationBubbleViews() {
    OfferNotificationBubbleControllerImpl* controller = GetController();
    if (!controller) {
      return nullptr;
    }
    return static_cast<OfferNotificationBubbleViews*>(
        controller->GetOfferNotificationBubbleView());
  }

  OfferNotificationBubbleControllerImpl* GetController() {
    return static_cast<OfferNotificationBubbleControllerImpl*>(
        OfferNotificationBubbleController::Get(GetWebContents()));
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OfferNotificationBubbleViewPixelBrowserTest,
    testing::Values(
        OfferNotificationBubbleViewPixelTestConfig{"FreeListingOffer_default"},
        OfferNotificationBubbleViewPixelTestConfig{
            "FreeListingOffer_on_navigation",
            absl::make_optional<std::vector<base::test::FeatureRefAndParams>>(
                {{commerce::kShowDiscountOnNavigation, {}}})},
        OfferNotificationBubbleViewPixelTestConfig{
            "FreeListingOffer_on_navigation_chrome_refresh_style",
            absl::make_optional<std::vector<base::test::FeatureRefAndParams>>(
                {{commerce::kShowDiscountOnNavigation, {}},
                 {::features::kChromeRefresh2023, {}}})}),
    GetTestName);

// TODO(crbug.com/1473417): Disabled because this is flaky on the bots, but not
// locally. Based on the logs, somehow the browser window becomes inactive
// during the test which causes the bubble not to show.
IN_PROC_BROWSER_TEST_P(OfferNotificationBubbleViewPixelBrowserTest,
                       DISABLED_InvokeUi_show_notification_bubble) {
  ShowAndVerifyUi();
}

}  // namespace autofill
