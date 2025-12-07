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
#include "chrome/browser/sync/device_info_sync_service_factory.h"
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
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"
#include "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"

using sync_preferences::CrossDevicePrefTracker;
using sync_preferences::TimestampedPrefValue;

namespace {

using desktop_to_mobile_promos::BubbleType;
using desktop_to_mobile_promos::PromoType;

// The time period over which the user has to have been active for at least 16
// days in order to be considered active on iOS.
const base::TimeDelta kActiveUserRecency = base::Days(28);

// Returns true if `time` is less time ago than `delta`.
bool IsRecent(base::Time time, base::TimeDelta delta) {
  return base::Time::Now() - time < delta;
}

// ShowIOSDesktopPromoBubble shows the iOS Desktop Promo Bubble based on the
// given promo type.
void ShowIOSDesktopPromoBubble(PromoType promo_type,
                               BubbleType bubble_type,
                               Profile* profile,
                               BrowserView* browser_view) {
  ToolbarButtonProvider* toolbar_button_provider =
      browser_view->toolbar_button_provider();
  switch (promo_type) {
    case PromoType::kPassword:
      IOSPromoBubble::ShowPromoBubble(
          {toolbar_button_provider->GetAnchorView(
              kActionShowPasswordsBubbleOrPage)},
          toolbar_button_provider->GetPageActionView(
              kActionShowPasswordsBubbleOrPage),
          profile, PromoType::kPassword, bubble_type);
      break;
    case PromoType::kAddress: {
      IOSPromoBubble::ShowPromoBubble(
          {toolbar_button_provider->GetAnchorView(
              kActionShowAddressesBubbleOrPage)},
          toolbar_button_provider->GetPageActionView(
              kActionShowAddressesBubbleOrPage),
          profile, PromoType::kAddress, bubble_type);
      break;
    }
    case PromoType::kPayment:
      IconLabelBubbleView* icon_view;
      if (IsPageActionMigrated(PageActionIconType::kSaveCard)) {
        icon_view = toolbar_button_provider->GetPageActionView(
            kActionShowPaymentsBubbleOrPage);
      } else {
        icon_view = toolbar_button_provider->GetPageActionIconView(
            PageActionIconType::kSaveCard);
      }
      CHECK(icon_view);

      IOSPromoBubble::ShowPromoBubble({toolbar_button_provider->GetAnchorView(
                                          kActionShowPaymentsBubbleOrPage)},
                                      icon_view, profile, PromoType::kPayment,
                                      bubble_type);
      break;
    case PromoType::kEnhancedBrowsing:
      IOSPromoBubble::ShowPromoBubble(
          {browser_view->toolbar()->app_menu_button()},
          /*highlighted_button=*/nullptr, profile, PromoType::kEnhancedBrowsing,
          bubble_type);
      break;
    case PromoType::kLens: {
      SidePanel* side_panel = browser_view->contents_height_side_panel();
      IOSPromoBubble::Anchor anchor = {side_panel};
      if (side_panel) {
        anchor.arrow = side_panel->IsRightAligned()
                           ? views::BubbleBorder::LEFT_CENTER
                           : views::BubbleBorder::RIGHT_CENTER;
      } else {
        anchor.view = browser_view->toolbar()->app_menu_button();
      }
      IOSPromoBubble::ShowPromoBubble(anchor,
                                      /*highlighted_button=*/nullptr, profile,
                                      PromoType::kLens, bubble_type);
      break;
    }
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
    PromoType promo_type,
    BubbleType bubble_type,
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
    const PromoType& promo_type,
    BubbleType bubble_type,
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

void VerifyIOSPromoEligibility(PromoType promo_type,
                               Browser* browser,
                               BubbleType bubble_type) {
  VerifyIOSPromoEligibilityCriteriaAsync(promo_type, bubble_type, browser);
}

void MaybeOverrideCardConfirmationBubbleWithIOSPaymentPromo(
    Browser* browser,
    base::OnceClosure promo_will_be_shown_callback,
    base::OnceClosure promo_not_shown_callback) {
  VerifyIOSPromoEligibilityCriteriaAsync(
      PromoType::kPayment, BubbleType::kQRCode, browser,
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

bool IsUserActiveOnAndroid(Profile* profile) {
  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  if (!device_info_sync_service) {
    return false;
  }

  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  if (!device_info_tracker) {
    return false;
  }

  for (const syncer::DeviceInfo* device_info :
       device_info_tracker->GetAllDeviceInfo()) {
    if (device_info->os_type() == syncer::DeviceInfo::OsType::kAndroid &&
        IsRecent(device_info->last_updated_timestamp(), kActiveUserRecency)) {
      return true;
    }
  }

  return false;
}

}  // namespace ios_promos_utils
