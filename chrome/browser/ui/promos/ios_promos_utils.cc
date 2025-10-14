// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promos_utils.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_utils.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/sync/prefs/cross_device_pref_tracker/cross_device_pref_tracker_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"
#include "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"

using sync_preferences::CrossDevicePrefTracker;
using sync_preferences::TimestampedPrefValue;

namespace {

// The time period over which the user has to have been active for at least 16
// days in order to be considered active on iOS.
const base::TimeDelta kActiveUserRecency = base::Days(28);

// Returns true if `time` is less time ago than `delta`.
bool IsRecent(base::Time time, base::TimeDelta delta) {
  return base::Time::Now() - time < delta;
}

// ShowIOSDesktopPromoBubble shows the iOS Desktop Promo Bubble based on the
// given promo type.
void ShowIOSDesktopPromoBubble(IOSPromoType promo_type,
                               IOSPromoBubbleType bubble_type,
                               Profile* profile,
                               BrowserView* browser_view) {
  ToolbarButtonProvider* toolbar_button_provider =
      browser_view->toolbar_button_provider();
  switch (promo_type) {
    case IOSPromoType::kPassword:
      IOSPromoBubble::ShowPromoBubble(
          toolbar_button_provider->GetAnchorView(
              kActionShowPasswordsBubbleOrPage),
          toolbar_button_provider->GetPageActionView(
              kActionShowPasswordsBubbleOrPage),
          profile, IOSPromoType::kPassword, bubble_type);
      break;
    case IOSPromoType::kAddress: {
      views::Button* highlighted_button =
          IsPageActionMigrated(PageActionIconType::kAutofillAddress)
              ? nullptr
              : toolbar_button_provider->GetPageActionIconView(
                    PageActionIconType::kAutofillAddress);

      IOSPromoBubble::ShowPromoBubble(toolbar_button_provider->GetAnchorView(
                                          kActionShowAddressesBubbleOrPage),
                                      highlighted_button, profile,
                                      IOSPromoType::kAddress, bubble_type);
      break;
    }
    case IOSPromoType::kPayment:
      IOSPromoBubble::ShowPromoBubble(
          toolbar_button_provider->GetAnchorView(
              kActionShowPaymentsBubbleOrPage),
          toolbar_button_provider->GetPageActionIconView(
              PageActionIconType::kSaveCard),
          profile, IOSPromoType::kPayment, bubble_type);
      break;
    case IOSPromoType::kEnhancedBrowsing:
      IOSPromoBubble::ShowPromoBubble(
          browser_view->toolbar()->app_menu_button(),
          /*highlighted_button=*/nullptr, profile,
          IOSPromoType::kEnhancedBrowsing, bubble_type);
      break;
    case IOSPromoType::kLens:
      IOSPromoBubble::ShowPromoBubble(
          browser_view->toolbar()->app_menu_button(),
          /*highlighted_button=*/nullptr, profile, IOSPromoType::kLens,
          bubble_type);
      break;
  }
}

// Runs a callback if it isn't null.
void RunCallback(std::optional<base::OnceClosure> callback) {
  if (callback) {
    (*std::exchange(callback, std::nullopt)).Run();
  }
}

// OnIOSPromoClassificationResult takes the result of the segmentation platform
// and computes, along with other criteria like impressions, whether the user
// should be shown the promo. If yes, attempts to show the promo.
void OnIOSPromoClassificationResult(
    IOSPromoType promo_type,
    IOSPromoBubbleType bubble_type,
    base::WeakPtr<Browser> browser,
    std::optional<base::OnceClosure> promo_will_be_shown_callback,
    std::optional<base::OnceClosure> promo_not_shown_callback,
    const segmentation_platform::ClassificationResult& result) {
  if (!browser) {
    RunCallback(std::move(promo_not_shown_callback));
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser->profile());

  if (promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result) &&
      tracker->ShouldTriggerHelpUI(
          promos_utils::GetIOSDesktopPromoFeatureEngagement(promo_type))) {
    RunCallback(std::move(promo_will_be_shown_callback));
    promos_utils::IOSDesktopPromoShown(browser->profile(), promo_type);
    ShowIOSDesktopPromoBubble(
        promo_type, bubble_type, browser->profile(),
        BrowserView::GetBrowserViewForBrowser(browser.get()));
    return;
  }

  RunCallback(std::move(promo_not_shown_callback));
}

void VerifyIOSPromoEligibilityCriteriaAsync(
    const IOSPromoType& promo_type,
    IOSPromoBubbleType bubble_type,
    Browser* browser,
    std::optional<base::OnceClosure> promo_will_be_shown_callback =
        std::nullopt,
    std::optional<base::OnceClosure> promo_not_shown_callback = std::nullopt) {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(browser->profile());

  // Verify that the user is currently syncing their preferences, hasn't
  // exceeded their impression limit, is not in the cooldown period or has not
  // opted-out from seeing the promo.
  if (sync_service && promos_utils::ShouldShowIOSDesktopPromo(
                          browser->profile(), sync_service, promo_type)) {
    auto input_context =
        base::MakeRefCounted<segmentation_platform::InputContext>();
    input_context->metadata_args.emplace(
        "active_days_limit", promos_utils::kiOSDesktopPromoLookbackWindow);
    input_context->metadata_args.emplace(
        "wait_for_device_info_in_seconds",
        segmentation_platform::processing::ProcessedValue(0));

    segmentation_platform::PredictionOptions options;
    options.on_demand_execution = true;

    // Get segmentation platform classification results and pass callback.
    segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
        browser->profile())
        ->GetClassificationResult(
            segmentation_platform::kDeviceSwitcherKey, options, input_context,
            base::BindOnce(&OnIOSPromoClassificationResult, promo_type,
                           bubble_type, browser->AsWeakPtr(),
                           std::move(promo_will_be_shown_callback),
                           std::move(promo_not_shown_callback)));
    return;
  }

  RunCallback(std::move(promo_not_shown_callback));
}

}  // namespace

namespace ios_promos_utils {

void VerifyIOSPromoEligibility(IOSPromoType promo_type,
                               Browser* browser,
                               IOSPromoBubbleType bubble_type) {
  VerifyIOSPromoEligibilityCriteriaAsync(promo_type, bubble_type, browser);
}

void MaybeOverrideCardConfirmationBubbleWithIOSPaymentPromo(
    Browser* browser,
    base::OnceClosure promo_will_be_shown_callback,
    base::OnceClosure promo_not_shown_callback) {
  VerifyIOSPromoEligibilityCriteriaAsync(
      IOSPromoType::kPayment, IOSPromoBubbleType::kQRCode, browser,
      std::move(promo_will_be_shown_callback),
      std::move(promo_not_shown_callback));
}

bool IsUserActiveOnIOS(Profile* profile) {
  CrossDevicePrefTracker* pref_tracker =
      CrossDevicePrefTrackerFactory::GetForProfile(profile);
  CHECK(pref_tracker);
  std::vector<TimestampedPrefValue> values = pref_tracker->GetValues(
      prefs::kCrossDeviceCrossPlatformPromosIOS16thActiveDay,
      /*filter=*/{syncer::DeviceInfo::OsType::kIOS});
  for (const TimestampedPrefValue& value : values) {
    std::optional<base::Time> date = base::ValueToTime(value.value);
    if (date.has_value() && IsRecent(date.value(), kActiveUserRecency)) {
      return true;
    }
  }
  return false;
}

}  // namespace ios_promos_utils
