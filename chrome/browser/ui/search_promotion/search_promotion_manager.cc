// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <string>

#include "base/notimplemented.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "url/gurl.h"

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

bool SearchPromotionManager::CheckEngagementLevel() {
  NOTIMPLEMENTED();
  return false;
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
