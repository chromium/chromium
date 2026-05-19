// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <string>
#include <utility>

#include "base/notimplemented.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "url/gurl.h"

namespace {

// Target group: 0 -> 8 days out of 28.
//
// We query SegmentationPlatformService using
// kChromeLowUserEngagementSegmentationKey, which tracks Session.TotalDuration
// over 28 days and classifies users into low user engagement if active fewer
// than 9 days out of the last 28 days. This model auto-executes and caches its
// results on browser startup, ensuring lookups during navigation are extremely
// fast and non-blocking.

}  // namespace

SearchPromotionManager::SearchPromotionManager(Profile& profile)
    : profile_(profile) {
  // SearchPromotionManager is currently a Windows-only feature intended to
  // promote specific search-related behaviors. On other platforms, the
  // manager remains inert.

  // Cache feature state to avoid repeated lookups on every navigation.
  is_promo_allowed_ = base::FeatureList::IsEnabled(
      feature_engagement::kIPHSearchPromotionFeature);
  if (is_promo_allowed_) {
    std::string arm_str = feature_engagement::kSearchPromotionArm.Get();
    if (arm_str == feature_engagement::kSearchPromotionArmA) {
      arm_ = feature_engagement::kSearchPromotionArmA;
    } else if (arm_str == feature_engagement::kSearchPromotionArmB) {
      arm_ = feature_engagement::kSearchPromotionArmB;
    }
  }
}

SearchPromotionManager::~SearchPromotionManager() = default;

void SearchPromotionManager::OnTargetURLVisited(const GURL& url) {
  if (!is_promo_allowed_) {
    return;
  }

  if (!IsEngagementLowEnough()) {
    return;
  }

  if (arm_ == feature_engagement::kSearchPromotionArmA) {
    PerformArmA();
  } else if (arm_ == feature_engagement::kSearchPromotionArmB) {
    PerformArmB();
  }
}

void SearchPromotionManager::OnPromoAccepted() {
  NOTIMPLEMENTED();
}

bool SearchPromotionManager::IsPromoAllowedForTesting() {
  return is_promo_allowed_;
}

bool SearchPromotionManager::IsEngagementLowEnoughForTesting() {
  return IsEngagementLowEnough();
}

bool SearchPromotionManager::IsEngagementLowEnough() {
  // Ensure we only target standard, persistent user profiles.
  // IsRegularProfile() filters out stuff like incognito, off the record
  // profiles, etc.
  if (!profile_->IsRegularProfile()) {
    return false;
  }

  auto* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          &profile_.get());
  if (!service) {
    return false;
  }

  // Use cached startup results.
  segmentation_platform::SegmentSelectionResult result =
      service->GetCachedSegmentResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey);

  if (!result.is_ready || !result.segment.has_value()) {
    return false;
  }

  return result.segment.value() ==
         segmentation_platform::proto::SegmentId::
             OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
}

void SearchPromotionManager::PerformArmA() {
  NOTIMPLEMENTED();
}

void SearchPromotionManager::PerformArmB() {
  NOTIMPLEMENTED();
}

void SearchPromotionManager::OnRegistryWriteComplete(const GURL& url,
                                                     bool success) {
  NOTIMPLEMENTED();
}
