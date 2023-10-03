// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/commerce/core/test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace autofill {

namespace {
constexpr char kTestURL[] = "https://www.example.com";
constexpr char kTestPromoCode[] = "FREEFALL1234";

AutofillOfferData CreateTestFreeListingCouponOffer(
    const std::vector<GURL>& merchant_origins,
    const std::string& promo_code) {
  int64_t offer_id = 2468;
  base::Time expiry = base::Time::Now() + base::Days(2);
  autofill::DisplayStrings display_strings;
  display_strings.value_prop_text = "5% off on shoes. Up to $50.";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";
  return autofill::AutofillOfferData::FreeListingCouponOffer(
      offer_id, expiry, merchant_origins, /*offer_details_url=*/GURL(),
      display_strings, promo_code);
}
}  // namespace

class OfferNotificationIconViewBrowserTest : public UiBrowserTest {
 public:
  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    AutofillOfferData offer = CreateTestFreeListingCouponOffer(
        /*merchant_origins=*/{GURL(kTestURL)}, kTestPromoCode);
    auto* autofill_client =
        ChromeAutofillClient::FromWebContentsForTesting(GetWebContents());

    if (name == "show_offer_notification_icon_only") {
      autofill_client->UpdateOfferNotification(&offer, {});
    } else if (name == "show_offer_notification_icon_expanded") {
      autofill_client->UpdateOfferNotification(
          &offer, {.expand_notification_icon = true});
    }
  }

  bool VerifyUi() override {
    auto* offer_notification_icon_view = GetIcon();
    if (!offer_notification_icon_view) {
      return false;
    }

    EXPECT_EQ(offer_notification_icon_view->GetAccessibleName(),
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    if (test_name == "InvokeUi_show_offer_notification_icon_only") {
      EXPECT_FALSE(offer_notification_icon_view->ShouldShowLabel());
    } else if (test_name == "InvokeUi_show_offer_notification_icon_expanded") {
      WaitForIconToFinishAnimating(offer_notification_icon_view);
      EXPECT_TRUE(offer_notification_icon_view->ShouldShowLabel());
      EXPECT_EQ(offer_notification_icon_view->GetIconLabelForTesting(),
                l10n_util::GetStringUTF16(IDS_DISCOUNT_ICON_EXPANDED_TEXT));
    }

    return true;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal. This is useful when using
    // the test-launcher-interactive option.
    ui_test_utils::WaitForBrowserToClose();
  }

 protected:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  OfferNotificationIconView* GetIcon() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(GetLocationBarView());
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kOfferNotificationChipElementId, context);

    return matched_view
               ? views::AsViewClass<OfferNotificationIconView>(matched_view)
               : nullptr;
  }

  void WaitForIconToFinishAnimating(OfferNotificationIconView* icon_view) {
    while (icon_view->is_animating_label()) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBarView* GetLocationBarView() {
    return GetBrowserView()->toolbar()->location_bar();
  }
};

IN_PROC_BROWSER_TEST_F(OfferNotificationIconViewBrowserTest,
                       InvokeUi_show_offer_notification_icon_only) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(OfferNotificationIconViewBrowserTest,
                       InvokeUi_show_offer_notification_icon_expanded) {
  ShowAndVerifyUi();
}
}  // namespace autofill
