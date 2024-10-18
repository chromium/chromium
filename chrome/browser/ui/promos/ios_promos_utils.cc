// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promos_utils.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/promos/promos_utils.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync/service/sync_service.h"

namespace {

// ShowIOSDesktopPromoBubble shows the iOS Desktop Promo Bubble based on the
// given promo type.
void ShowIOSDesktopPromoBubble(IOSPromoType promo_type,
                               Profile* profile,
                               ToolbarButtonProvider* toolbar_button_provider) {
  switch (promo_type) {
    case IOSPromoType::kPassword:
      IOSPromoBubble::ShowPromoBubble(
          toolbar_button_provider->GetAnchorView(
              PageActionIconType::kManagePasswords),
          toolbar_button_provider->GetPageActionIconView(
              PageActionIconType::kManagePasswords),
          profile, IOSPromoType::kPassword);
      break;
    case IOSPromoType::kAddress:
      IOSPromoBubble::ShowPromoBubble(
          toolbar_button_provider->GetAnchorView(
              PageActionIconType::kAutofillAddress),
          toolbar_button_provider->GetPageActionIconView(
              PageActionIconType::kAutofillAddress),
          profile, IOSPromoType::kAddress);
      break;
    case IOSPromoType::kPayment:
      IOSPromoBubble::ShowPromoBubble(
          toolbar_button_provider->GetAnchorView(PageActionIconType::kSaveCard),
          toolbar_button_provider->GetPageActionIconView(
              PageActionIconType::kSaveCard),
          profile, IOSPromoType::kPayment);
      break;
  }
}

// OnIOSPromoClassificationResult takes the result of the segmentation platform
// and computes, along with other criteria like impressions, whether the user
// should be shown the promo. If yes, attempts to show the promo.
void OnIOSPromoClassificationResult(
    IOSPromoType promo_type,
    base::WeakPtr<Profile> profile,
    ToolbarButtonProvider* toolbar_button_provider,
    const segmentation_platform::ClassificationResult& result) {
  Profile* profile_ptr = profile.get();
  if (!profile_ptr) {
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_ptr);

  if (promos_utils::UserNotClassifiedAsMobileDeviceSwitcher(result) &&
      tracker->ShouldTriggerHelpUI(
          promos_utils::GetIOSDesktopPromoFeatureEngagement(promo_type))) {
    promos_utils::IOSDesktopPromoShown(profile_ptr, promo_type);
    ShowIOSDesktopPromoBubble(promo_type, profile_ptr, toolbar_button_provider);
  }
}

}  // namespace

namespace ios_promos_utils {

void VerifyIOSPromoEligibility(IOSPromoType promo_type,
                               Profile* profile,
                               ToolbarButtonProvider* toolbar_button_provider) {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  // Verify that the user is currently syncing their preferences, hasn't
  // exceeded their impression limit, is not in the cooldown period or has not
  // opted-out from seeing the promo.
  if (sync_service && promos_utils::ShouldShowIOSDesktopPromo(
                          profile, sync_service, promo_type)) {
    auto input_context =
        base::MakeRefCounted<segmentation_platform::InputContext>();
    input_context->metadata_args.emplace(
        "active_days_limit",
        promos_utils::kiOSDesktopPromoLookbackWindow.InDays());
    input_context->metadata_args.emplace(
        "wait_for_device_info_in_seconds",
        segmentation_platform::processing::ProcessedValue(0));

    segmentation_platform::PredictionOptions options;
    options.on_demand_execution = true;

    // Get segmentation platform classification results and pass callback.
    segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
        profile)
        ->GetClassificationResult(
            segmentation_platform::kDeviceSwitcherKey, options, input_context,
            base::BindOnce(&OnIOSPromoClassificationResult, promo_type,
                           profile->GetWeakPtr(), toolbar_button_provider));
  }
}

}  // namespace ios_promos_utils
