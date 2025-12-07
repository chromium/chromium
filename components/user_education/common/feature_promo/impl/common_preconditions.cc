// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/common_preconditions.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_context.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/typed_data_collection.h"
#include "ui/base/interaction/typed_identifier.h"

namespace user_education {

DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kFeatureEnabledPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kFeatureEngagementTrackerInitializedPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kMeetsFeatureEngagementCriteriaPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kContextValidPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kAnchorElementPrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kLifecyclePrecondition);
DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kSessionPolicyPrecondition);

FeatureEnabledPrecondition::FeatureEnabledPrecondition(
    const base::Feature& feature)
    : CachingFeaturePromoPrecondition(
          kFeatureEnabledPrecondition,
          "Feature Enabled",
          base::FeatureList::IsEnabled(feature)
              ? FeaturePromoResult::Success()
              : FeaturePromoResult::kFeatureDisabled) {}

FeatureEnabledPrecondition::~FeatureEnabledPrecondition() = default;

FeatureEngagementTrackerInitializedPrecondition::
    FeatureEngagementTrackerInitializedPrecondition(
        feature_engagement::Tracker* tracker)
    : CachingFeaturePromoPrecondition(
          kFeatureEngagementTrackerInitializedPrecondition,
          "Feature Engagement Tracker Initialized",
          FeaturePromoResult::kBlockedByConfig) {
  if (tracker) {
    // Want to return true immediately if the tracker is initialized so spot
    // queries (e.g. `CanShowPromo()`) won't fail.
    if (tracker->IsInitialized()) {
      OnFeatureEngagementTrackerInitialized(true);
    } else {
      // Otherwise, wait for initialization.
      tracker->AddOnInitializedCallback(
          base::BindOnce(&FeatureEngagementTrackerInitializedPrecondition::
                             OnFeatureEngagementTrackerInitialized,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    // If no tracker, then it cannot be initialized.
    OnFeatureEngagementTrackerInitialized(false);
  }
}

FeatureEngagementTrackerInitializedPrecondition::
    ~FeatureEngagementTrackerInitializedPrecondition() = default;

void FeatureEngagementTrackerInitializedPrecondition::
    OnFeatureEngagementTrackerInitialized(
        bool tracker_initialized_successfully) {
  if (tracker_initialized_successfully) {
    set_check_result(FeaturePromoResult::Success());
  } else {
    set_check_result(FeaturePromoResult::kError);
  }
}

MeetsFeatureEngagementCriteriaPrecondition::
    MeetsFeatureEngagementCriteriaPrecondition(
        const base::Feature& feature,
        const feature_engagement::Tracker& tracker)
    : FeaturePromoPreconditionBase(
          kMeetsFeatureEngagementCriteriaPrecondition,
          base::StringPrintf("Feature %s Meets Feature Engagement Criteria",
                             feature.name)),
      feature_(feature),
      tracker_(tracker) {}

MeetsFeatureEngagementCriteriaPrecondition::
    ~MeetsFeatureEngagementCriteriaPrecondition() = default;

FeaturePromoResult
MeetsFeatureEngagementCriteriaPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection& data) const {
  if (tracker_->IsInitialized()) {
    // Note: if we don't have access to `ListEvents()` this is a no-op.
#if !BUILDFLAG(IS_ANDROID)
    for (const auto& [config, count] : tracker_->ListEvents(*feature_)) {
      if (!config.comparator.MeetsCriteria(count)) {
        return FeaturePromoResult::kBlockedByConfig;
      }
    }
#endif
  }
  return FeaturePromoResult::Success();
}

ContextValidPrecondition::ContextValidPrecondition(
    const UserEducationContextPtr& context)
    : FeaturePromoPreconditionBase(kContextValidPrecondition, "Context Valid"),
      context_(context) {
  CHECK(context_) << "Must specify a context when creating precondition.";
  CHECK(context_->IsValid())
      << "Context must be valid when precondition is created.";
}
ContextValidPrecondition::~ContextValidPrecondition() = default;

FeaturePromoResult ContextValidPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection&) const {
  return context_->IsValid() ? FeaturePromoResult::Success()
                             : FeaturePromoResult::kAnchorNotVisible;
}

DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(AnchorElementPrecondition,
                                    std::optional<int>,
                                    kRotatingPromoIndex);

DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(AnchorElementPrecondition,
                                    ui::SafeElementReference,
                                    kAnchorElement);

AnchorElementPrecondition::AnchorElementPrecondition(
    const AnchorElementProvider& provider,
    ui::ElementContext default_context,
    bool pre_increment_index)
    : FeaturePromoPreconditionBase(kAnchorElementPrecondition,
                                   "Anchor Element Visible"),
      provider_(provider),
      default_context_(default_context),
      pre_increment_index_(pre_increment_index) {
  InitCache(kAnchorElement, kRotatingPromoIndex);
}

AnchorElementPrecondition::~AnchorElementPrecondition() = default;

FeaturePromoResult AnchorElementPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection& data) const {
  const auto& lifecycle = data[LifecyclePrecondition::kLifecycle];
  std::optional<int> index;
  if (lifecycle->promo_type() ==
      FeaturePromoSpecification::PromoType::kRotating) {
    int next_index = lifecycle->GetPromoIndex();
    if (pre_increment_index_) {
      next_index = (next_index + 1) % lifecycle->num_rotating_entries();
    }
    index = provider_->GetNextValidIndex(next_index);
  }
  GetCachedDataForComputation(data, kRotatingPromoIndex) = index;
  auto* const element = provider_->GetAnchorElement(default_context_, index);
  GetCachedDataForComputation(data, kAnchorElement) = element;
  return element ? FeaturePromoResult::Success()
                 : FeaturePromoResult::kAnchorNotVisible;
}

DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(LifecyclePrecondition,
                                    std::unique_ptr<FeaturePromoLifecycle>,
                                    kLifecycle);

LifecyclePrecondition::LifecyclePrecondition(
    std::unique_ptr<FeaturePromoLifecycle> lifecycle,
    bool for_demo)
    : FeaturePromoPreconditionBase(kLifecyclePrecondition, "Lifecycle Check"),
      for_demo_(for_demo) {
  CHECK(lifecycle);
  InitCachedData(kLifecycle, std::move(lifecycle));
}

LifecyclePrecondition::~LifecyclePrecondition() = default;

FeaturePromoResult LifecyclePrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection& data) const {
  auto* const lifecycle = GetCachedDataForComputation(data, kLifecycle).get();
  return for_demo_ ? FeaturePromoResult::Success() : lifecycle->CanShow();
}

SessionPolicyPrecondition::SessionPolicyPrecondition(
    FeaturePromoSessionPolicy* session_policy,
    FeaturePromoPriorityProvider::PromoPriorityInfo priority_info,
    GetCurrentPromoInfoCallback get_current_promo_info_callback)
    : FeaturePromoPreconditionBase(kSessionPolicyPrecondition,
                                   "Session Policy Check"),
      session_policy_(*session_policy),
      priority_info_(priority_info),
      get_current_promo_info_callback_(
          std::move(get_current_promo_info_callback)) {}

SessionPolicyPrecondition::~SessionPolicyPrecondition() = default;

// FeaturePromoPrecondition:
FeaturePromoResult SessionPolicyPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection& data) const {
  return session_policy_->CanShowPromo(priority_info_,
                                       get_current_promo_info_callback_.Run());
}

}  // namespace user_education
