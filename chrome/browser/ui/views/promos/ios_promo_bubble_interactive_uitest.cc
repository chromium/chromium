// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/desktop_to_mobile_promos/promos_pref_names.h"
#include "chrome/browser/desktop_to_mobile_promos/promos_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interaction_sequence.h"

using desktop_to_mobile_promos::BubbleType;
using desktop_to_mobile_promos::PromoType;

// Test suite for the desktop-to-iOS promo bubbles.
class IOSPromoBubbleBrowserTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<std::tuple<PromoType, bool>> {
 public:
  IOSPromoBubbleBrowserTest() = default;
  ~IOSPromoBubbleBrowserTest() override = default;

  void SetUp() override {
    // Enable the non-IPH features.
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {kMobilePromoOnDesktopWithReminder,
         {{kMobilePromoOnDesktopPromoTypeParam, "2"}}},
        {sync_preferences::features::kEnableCrossDevicePrefTracker, {}}};
    std::vector<base::test::FeatureRef> disabled_features = {};

    if (IsWalletBrandingV2Enabled()) {
      enabled_features.push_back(
          {autofill::features::kAutofillEnableWalletBranding, {}});
      enabled_features.push_back(
          {autofill::features::kAutofillEnableWalletBrandingV2, {}});
    } else {
      disabled_features.emplace_back(
          autofill::features::kAutofillEnableWalletBrandingV2);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);

    // Enable the IPH features.
    scoped_iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHiOSLensPromoDesktopFeature,
         feature_engagement::kIPHiOSEnhancedBrowsingDesktopFeature,
         feature_engagement::kIPHiOSPasswordPromoDesktopFeature,
         feature_engagement::kIPHiOSAddressPromoDesktopFeature,
         feature_engagement::kIPHiOSPaymentPromoDesktopFeature});

    InteractiveBrowserTest::SetUp();
  }

  static constexpr char kScreenshotBaselineCL[] = "7064212";

  auto ShowPromoBubble(BubbleType bubble_type) {
    return base::BindLambdaForTesting([=, this]() {
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser());
      ToolbarButtonProvider* button_provider =
          browser_view->toolbar_button_provider();

      PromoType promo_type = GetPromoType();
      views::BubbleAnchor anchor;
      views::Button* highlighted_button = nullptr;
      std::optional<ui::ElementIdentifier> highlighted_element;

      // Explicitly set impression count to 0 before showing the promo.
      // This ensures that the first impression is recorded as 1.
      promos_utils::IOSPromoPrefsConfig promo_prefs(promo_type);
      browser()->profile()->GetPrefs()->SetInteger(
          promo_prefs.promo_impressions_counter_pref_name, 0);

      switch (promo_type) {
        case PromoType::kPassword:
          anchor = button_provider->GetBubbleAnchor(
              kActionShowPasswordsBubbleOrPage);
          highlighted_button = button_provider->GetPageActionView(
              kActionShowPasswordsBubbleOrPage);
          highlighted_element = kPasswordsOmniboxKeyIconElementId;
          break;
        case PromoType::kAddress:
          anchor = button_provider->GetBubbleAnchor(
              kActionShowAddressesBubbleOrPage);
          highlighted_button = button_provider->GetPageActionView(
              kActionShowAddressesBubbleOrPage);
          highlighted_element = kAutofillAddressPageActionElementId;
          break;
        case PromoType::kPayment:
          anchor =
              button_provider->GetBubbleAnchor(kActionShowPaymentsBubbleOrPage);
          highlighted_button =
              button_provider->GetPageActionView(kActionShowPaymentsBubbleOrPage);
          highlighted_element = kAutofillSavePaymentsPageActionElementId;
          break;
        case PromoType::kEnhancedBrowsing:
        case PromoType::kLens:
          anchor = button_provider->GetAppMenuControl()->GetAnchor();
          break;
        default:
          NOTREACHED();
      }

      promos_utils::IOSDesktopPromoShown(browser()->profile(), promo_type);
      IOSPromoBubble::ShowPromoBubble(
          IOSPromoBubble::Anchor{anchor}, highlighted_button,
          highlighted_element, browser()->profile(), promo_type, bubble_type);
    });
  }

  PromoType GetPromoType() { return std::get<0>(GetParam()); }
  bool IsWalletBrandingV2Enabled() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;
};

IN_PROC_BROWSER_TEST_P(IOSPromoBubbleBrowserTest, ShowQRCode) {
  if (GetPromoType() == PromoType::kAddress) {
    GTEST_SKIP() << "kAutofillAddress is migrated, use ShowQRCode_NoPageAction instead.";
  }

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Kombucha screenshots not supported on all bots"),
      Do(ShowPromoBubble(BubbleType::kQRCode)),
      WaitForShow(kIOSPromoBubbleElementId),
      Screenshot(kIOSPromoBubbleElementId, "QRCode", kScreenshotBaselineCL));
}

IN_PROC_BROWSER_TEST_P(IOSPromoBubbleBrowserTest, ShowQRCode_NoPageAction) {
  if (GetPromoType() != PromoType::kAddress) {
    GTEST_SKIP() << "Test is only for DesktopToIOSPromoType::kAddress.";
  }

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Kombucha screenshots not supported on all bots"),
      Do(ShowPromoBubble(BubbleType::kQRCode)),
      WaitForShow(kIOSPromoBubbleElementId),
      Screenshot(kIOSPromoBubbleElementId, "QRCode_NoPageAction",
                 kScreenshotBaselineCL));
}

#if BUILDFLAG(IS_WIN)
// Disabled by gardener on 02/10/2026.
// https://crbug.com/483422434.
#define MAYBE_ShowReminder DISABLED_ShowReminder
#else
#define MAYBE_ShowReminder ShowReminder
#endif
IN_PROC_BROWSER_TEST_P(IOSPromoBubbleBrowserTest, MAYBE_ShowReminder) {
  if (GetPromoType() == PromoType::kAddress ||
      GetPromoType() == PromoType::kPayment) {
    GTEST_SKIP() << "Reminder bubble not supported for this promo type.";
  }

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Kombucha screenshots not supported on all bots"),
      Do(ShowPromoBubble(BubbleType::kReminder)),
      WaitForShow(kIOSPromoBubbleElementId),
      Screenshot(kIOSPromoBubbleElementId, "Reminder", kScreenshotBaselineCL));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IOSPromoBubbleBrowserTest,
    testing::Combine(testing::Values(PromoType::kPassword,
                                     PromoType::kAddress,
                                     PromoType::kPayment,
                                     PromoType::kEnhancedBrowsing,
                                     PromoType::kLens),
                     testing::Bool()));
