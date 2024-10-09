// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_service.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_education/recent_session_tracker.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/new_badge_controller.h"
#include "components/user_education/common/new_badge_policy.h"
#include "components/user_education/common/user_education_features.h"

BASE_FEATURE(kAllowRecentSessionTracking,
             "AllowRecentSessionTracking",
             base::FEATURE_ENABLED_BY_DEFAULT);

UserEducationService::UserEducationService(
    std::unique_ptr<BrowserFeaturePromoStorageService> storage_service,
    bool allows_promos)
    : tutorial_service_(&tutorial_registry_, &help_bubble_factory_registry_),
      feature_promo_storage_service_(std::move(storage_service)),
      feature_promo_session_policy_(
          user_education::features::IsUserEducationV2()
              ? std::make_unique<user_education::FeaturePromoSessionPolicyV2>()
              : std::make_unique<user_education::FeaturePromoSessionPolicy>()) {
  feature_promo_session_policy_->Init(&feature_promo_session_manager_,
                                      feature_promo_storage_service_.get());
  if (allows_promos) {
    new_badge_registry_ = std::make_unique<user_education::NewBadgeRegistry>();
    new_badge_controller_ =
        std::make_unique<user_education::NewBadgeController>(
            *new_badge_registry_, *feature_promo_storage_service_,
            std::make_unique<user_education::NewBadgePolicy>());
  }

  if (base::FeatureList::IsEnabled(kAllowRecentSessionTracking)) {
    // Only create the recent session tracker if recent session tracking is
    // allowed (default).
    recent_session_tracker_ = std::make_unique<RecentSessionTracker>(
        feature_promo_session_manager_, *feature_promo_storage_service_,
        *feature_promo_storage_service_);
  } else {
    // If the feature is disabled, ensure that we clear any old data.
    feature_promo_storage_service_->ResetRecentSessionData();
  }
}

// static
user_education::DisplayNewBadge UserEducationService::MaybeShowNewBadge(
    content::BrowserContext* context,
    const base::Feature& feature) {
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(context);
  if (!service || !service->new_badge_controller()) {
    return user_education::DisplayNewBadge();
  }

  // For some tests, browser initialization is never done so there are no
  // registered "New" Badges.
  if (!service->new_badge_registry()->IsFeatureRegistered(
          user_education::features::kNewBadgeTestFeature)) {
    // Verify that this is actually a testing situation, and then fail.
    CHECK(Profile::FromBrowserContext(context)->AsTestingProfile());
    return user_education::DisplayNewBadge();
  }

  return service->new_badge_controller()->MaybeShowNewBadge(feature);
}

// static
void UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
    content::BrowserContext* context,
    const base::Feature& feature) {
  // Do not register events for disabled features.
  if (!base::FeatureList::IsEnabled(feature)) {
    return;
  }

  // Do not register events for profiles incompatible with user education.
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(context);
  if (!service || !service->new_badge_controller()) {
    return;
  }

  // Notify the "New" Badge controller.
  service->new_badge_controller()->NotifyFeatureUsedIfValid(feature);
}

UserEducationService::~UserEducationService() = default;
