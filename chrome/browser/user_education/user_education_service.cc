// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_service.h"

#include <memory>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/recent_session_tracker.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"
#include "components/user_education/common/new_badge/new_badge_policy.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"

BASE_FEATURE(kAllowRecentSessionTracking, base::FEATURE_ENABLED_BY_DEFAULT);

UserEducationService::UserEducationService(Profile* profile, bool allows_promos)
    : profile_(*profile),
      tutorial_service_(&tutorial_registry_, &help_bubble_factory_registry_),
      user_education_storage_service_(
          std::make_unique<BrowserUserEducationStorageService>(profile)),
      feature_promo_session_policy_(
          std::make_unique<user_education::FeaturePromoSessionPolicyV2>()) {
  feature_promo_session_policy_->Init(&user_education_session_manager_,
                                      user_education_storage_service_.get());
  product_messaging_controller_.Init(user_education_session_manager_,
                                     *user_education_storage_service_);

  if (allows_promos) {
    new_badge_registry_ = std::make_unique<user_education::NewBadgeRegistry>();
    new_badge_controller_ =
        std::make_unique<user_education::NewBadgeController>(
            *new_badge_registry_, *user_education_storage_service_,
            std::make_unique<user_education::NewBadgePolicy>());
  }

  if (base::FeatureList::IsEnabled(kAllowRecentSessionTracking)) {
    // Only create the recent session tracker if recent session tracking is
    // allowed (default).
    recent_session_tracker_ = std::make_unique<RecentSessionTracker>(
        user_education_session_manager_, *user_education_storage_service_,
        *user_education_storage_service_);
  } else {
    // If the feature is disabled, ensure that we clear any old data.
    user_education_storage_service_->ResetRecentSessionData();
  }

  if (allows_promos &&
      user_education::features::GetNtpBrowserPromoType() !=
          user_education::features::NtpBrowserPromoType::kNone) {
    ntp_promo_registry_ = std::make_unique<user_education::NtpPromoRegistry>();
    ntp_promo_controller_ =
        std::make_unique<user_education::NtpPromoController>(
            *ntp_promo_registry_, *user_education_storage_service_,
            user_education::GetNtpPromoControllerParams());
  }

  // This MUST be last, after all other initialization, because it relies on
  // members initialized above.
  if (allows_promos) {
    if (feature_promo_controller_) {
      CHECK_IS_TEST()
          << "The controller may only be set once in production code.";
    } else {
      feature_promo_controller_ = CreateUserEducationResources(*this);
    }
  }
}

void UserEducationService::Shutdown() {
  // This holds some references that may be dangerous to hang onto during
  // teardown, so free them now.
  feature_promo_controller_.reset();
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
