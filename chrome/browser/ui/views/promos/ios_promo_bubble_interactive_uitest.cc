// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
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
      public testing::WithParamInterface<PromoType> {
 public:
  IOSPromoBubbleBrowserTest() = default;
  ~IOSPromoBubbleBrowserTest() override = default;

  void SetUp() override {
    // Enable the non-IPH features.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kMobilePromoOnDesktop, {{kMobilePromoOnDesktopPromoTypeParam, "2"}}},
         {sync_preferences::features::kEnableCrossDevicePrefTracker, {}}},
        {});

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

      PromoType promo_type = GetParam();
      views::View* anchor_view = nullptr;
      views::Button* highlighted_button = nullptr;

      // Explicitly set impression count to 0 before showing the promo.
      // This ensures that the first impression is recorded as 1.
      promos_utils::IOSPromoPrefsConfig promo_prefs(promo_type);
      browser()->profile()->GetPrefs()->SetInteger(
          promo_prefs.promo_impressions_counter_pref_name, 0);

      switch (promo_type) {
        case PromoType::kPassword:
          anchor_view =
              button_provider->GetAnchorView(kActionShowPasswordsBubbleOrPage);
          highlighted_button = button_provider->GetPageActionView(
              kActionShowPasswordsBubbleOrPage);
          break;
        case PromoType::kAddress:
          anchor_view =
              button_provider->GetAnchorView(kActionShowAddressesBubbleOrPage);
          highlighted_button = button_provider->GetPageActionIconView(
              PageActionIconType::kAutofillAddress);
          break;
        case PromoType::kPayment:
          anchor_view =
              button_provider->GetAnchorView(kActionShowPaymentsBubbleOrPage);
          highlighted_button = button_provider->GetPageActionIconView(
              PageActionIconType::kSaveCard);
          break;
        case PromoType::kEnhancedBrowsing:
        case PromoType::kLens:
          anchor_view = browser_view->toolbar()->app_menu_button();
          break;
        default:
          NOTREACHED();
      }

      promos_utils::IOSDesktopPromoShown(browser()->profile(), promo_type);
      IOSPromoBubble::ShowPromoBubble(IOSPromoBubble::Anchor{anchor_view},
                                      highlighted_button, browser()->profile(),
                                      promo_type, bubble_type);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;
};

IN_PROC_BROWSER_TEST_P(IOSPromoBubbleBrowserTest, ShowQRCode) {
  if (GetParam() == PromoType::kAddress) {
    if (IsPageActionMigrated(PageActionIconType::kAutofillAddress)) {
      GTEST_SKIP() << "This test is for the non-migrated state.";
    }
  }

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Kombucha screenshots not supported on all bots"),
      Do(ShowPromoBubble(BubbleType::kQRCode)),
      WaitForShow(kIOSPromoBubbleElementId),
      Screenshot(kIOSPromoBubbleElementId, "QRCode", kScreenshotBaselineCL));
}

IN_PROC_BROWSER_TEST_P(IOSPromoBubbleBrowserTest, ShowQRCode_NoPageAction) {
  if (GetParam() != PromoType::kAddress) {
    GTEST_SKIP() << "Test is only for DesktopToIOSPromoType::kAddress.";
  }

  if (!IsPageActionMigrated(PageActionIconType::kAutofillAddress)) {
    GTEST_SKIP() << "This test is for the migrated state.";
  }

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Kombucha screenshots not supported on all bots"),
      Do(ShowPromoBubble(BubbleType::kQRCode)),
      WaitForShow(kIOSPromoBubbleElementId),
      Screenshot(kIOSPromoBubbleElementId, "QRCode_NoPageAction",
                 kScreenshotBaselineCL));
}

IN_PROC_BROWSER_TEST_P(IOSPromoBubbleBrowserTest, ShowReminder) {
  if (GetParam() == PromoType::kAddress || GetParam() == PromoType::kPayment) {
    GTEST_SKIP() << "Reminder bubble not supported for this promo type.";
  }

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Kombucha screenshots not supported on all bots"),
      Do(ShowPromoBubble(BubbleType::kReminder)),
      WaitForShow(kIOSPromoBubbleElementId),
      Screenshot(kIOSPromoBubbleElementId, "Reminder", kScreenshotBaselineCL));
}

INSTANTIATE_TEST_SUITE_P(All,
                         IOSPromoBubbleBrowserTest,
                         testing::Values(PromoType::kPassword,
                                         PromoType::kAddress,
                                         PromoType::kPayment,
                                         PromoType::kEnhancedBrowsing,
                                         PromoType::kLens));
