// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/common_preconditions.h"

#include "base/functional/bind.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace user_education {

DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kFeatureEngagementTrackerInitializedPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kMeetsFeatureEngagementCriteriaPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kAnchorElementPrecondition);

FeatureEngagementTrackerInitializedPrecondition::
    FeatureEngagementTrackerInitializedPrecondition(
        feature_engagement::Tracker* tracker)
    : CachingFeaturePromoPrecondition(
          kFeatureEngagementTrackerInitializedPrecondition,
          FeaturePromoResult::kBlockedByConfig,
          "Feature Engagement Tracker Initialized",
          false) {
  if (tracker) {
    tracker->AddOnInitializedCallback(
        base::BindOnce(&FeatureEngagementTrackerInitializedPrecondition::
                           OnFeatureEngagementTrackerInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnFeatureEngagementTrackerInitialized(false);
  }
}

FeatureEngagementTrackerInitializedPrecondition::
    ~FeatureEngagementTrackerInitializedPrecondition() = default;

void FeatureEngagementTrackerInitializedPrecondition::
    OnFeatureEngagementTrackerInitialized(
        bool tracker_initialized_successfully) {
  if (tracker_initialized_successfully) {
    set_is_allowed(true);
  } else {
    set_failure(FeaturePromoResult::kError);
  }
}

MeetsFeatureEngagementCriteriaPrecondition::
    MeetsFeatureEngagementCriteriaPrecondition(
        const base::Feature& feature,
        const feature_engagement::Tracker& tracker)
    : FeaturePromoPreconditionBase(
          kMeetsFeatureEngagementCriteriaPrecondition,
          FeaturePromoResult::kBlockedByConfig,
          base::StringPrintf("Feature %s Meets Feature Engagement Criteria",
                             feature.name)),
      feature_(feature),
      tracker_(tracker) {}

MeetsFeatureEngagementCriteriaPrecondition::
    ~MeetsFeatureEngagementCriteriaPrecondition() = default;

bool MeetsFeatureEngagementCriteriaPrecondition::IsAllowed() const {
  // Note: if we don't have access to `ListEvents()` this is a no-op.
#if !BUILDFLAG(IS_ANDROID)
  for (const auto& [config, count] : tracker_->ListEvents(*feature_)) {
    if (!config.comparator.MeetsCriteria(count)) {
      return false;
    }
  }
#endif
  return true;
}

AnchorElementPrecondition::AnchorElementPrecondition(
    const AnchorElementProvider& provider,
    ui::ElementContext default_context)
    : FeaturePromoPreconditionBase(kAnchorElementPrecondition,
                                   FeaturePromoResult::kBlockedByUi,
                                   "Anchor Element Visible"),
      provider_(provider),
      default_context_(default_context) {}

AnchorElementPrecondition::~AnchorElementPrecondition() = default;

bool AnchorElementPrecondition::IsAllowed() const {
  return nullptr != provider_->GetAnchorElement(default_context_);
}

}  // namespace user_education
