// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_policy.h"

#include "base/metrics/field_trial_params.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"

namespace user_education {

namespace {

enum class PromoPriority { kNone, kLow, kMedium, kHigh };

}  // namespace

FeaturePromoSessionPolicy::FeaturePromoSessionPolicy() = default;
FeaturePromoSessionPolicy::~FeaturePromoSessionPolicy() = default;

void FeaturePromoSessionPolicy::Init(
    FeaturePromoSessionManager* session_manager,
    FeaturePromoStorageService* storage_service) {
  session_manager_ = session_manager;
  storage_service_ = storage_service;
}

void FeaturePromoSessionPolicy::NotifyPromoShown(const PromoInfo& promo_shown) {
  current_promo_shown_time_ = storage_service_->GetCurrentTime();
}

void FeaturePromoSessionPolicy::NotifyPromoEnded(
    const PromoInfo& promo_ended,
    FeaturePromoClosedReason close_reason) {
  // The close time may already have been recorded; for example, when a bubble
  // is closed but the promo continues and then ends later.
  if (!current_promo_shown_time_) {
    return;
  }

  // Save and reset the show time.
  const base::Time show_time = *current_promo_shown_time_;
  current_promo_shown_time_.reset();

  // Lightweight promos don't count for cooldown.
  if (promo_ended.weight != PromoWeight::kHeavy) {
    return;
  }

  switch (close_reason) {
    case FeaturePromoClosedReason::kDismiss:
    case FeaturePromoClosedReason::kSnooze:
    case FeaturePromoClosedReason::kAction:
    case FeaturePromoClosedReason::kCancel:
    case FeaturePromoClosedReason::kFeatureEngaged: {
      // Thees all count as active user dismiss, so full cooldown applies.
      auto new_data = storage_service_->ReadPolicyData();
      new_data.last_heavyweight_promo_time = show_time;
      storage_service_->SavePolicyData(new_data);
      break;
    }
    case FeaturePromoClosedReason::kTimeout:
    case FeaturePromoClosedReason::kAbortPromo:
    case FeaturePromoClosedReason::kOverrideForDemo:
    case FeaturePromoClosedReason::kOverrideForPrecedence:
    case FeaturePromoClosedReason::kOverrideForTesting:
    case FeaturePromoClosedReason::kOverrideForUIRegionConflict:
    case FeaturePromoClosedReason::kAbortedByFeature:
    case FeaturePromoClosedReason::kAbortedByAnchorHidden:
    case FeaturePromoClosedReason::kAbortedByBubbleDestroyed:
      // These count as the user not interacting, so they cannot trigger a full
      // cooldown. Do not record the shown time.
      break;
  }
}

FeaturePromoResult FeaturePromoSessionPolicy::CanShowPromo(
    PromoInfo to_show,
    std::optional<PromoInfo> currently_showing) const {
  return (!currently_showing || to_show.priority > currently_showing->priority)
             ? FeaturePromoResult::Success()
             : FeaturePromoResult::kBlockedByPromo;
}

FeaturePromoSessionPolicyV2::FeaturePromoSessionPolicyV2()
    : FeaturePromoSessionPolicyV2(features::GetSessionStartGracePeriod(),
                                  features::GetLowPriorityCooldown()) {}

FeaturePromoSessionPolicyV2::FeaturePromoSessionPolicyV2(
    base::TimeDelta session_start_grace_period,
    base::TimeDelta heavyweight_promo_cooldown)
    : session_start_grace_period_(session_start_grace_period),
      heavyweight_promo_cooldown_(heavyweight_promo_cooldown) {}

FeaturePromoSessionPolicyV2::~FeaturePromoSessionPolicyV2() = default;

FeaturePromoSessionPolicy::PromoInfo
FeaturePromoSessionPolicy::SpecificationToPromoInfo(
    const FeaturePromoSpecification& spec) const {
  PromoInfo promo_info;
  switch (spec.promo_subtype()) {
    case FeaturePromoSpecification::PromoSubtype::kLegalNotice:
      promo_info.priority = PromoPriority::kHigh;
      break;
    case FeaturePromoSpecification::PromoSubtype::kActionableAlert:
    case FeaturePromoSpecification::PromoSubtype::kKeyedNotice:
      promo_info.priority = PromoPriority::kMedium;
      break;
    case FeaturePromoSpecification::PromoSubtype::kNormal:
      promo_info.priority = PromoPriority::kLow;
      break;
  }
  switch (spec.promo_type()) {
    case FeaturePromoSpecification::PromoType::kToast:
    case FeaturePromoSpecification::PromoType::kLegacy:
    case FeaturePromoSpecification::PromoType::kRotating:
      promo_info.weight = PromoWeight::kLight;
      break;
    case FeaturePromoSpecification::PromoType::kSnooze:
    case FeaturePromoSpecification::PromoType::kTutorial:
    case FeaturePromoSpecification::PromoType::kCustomAction:
      promo_info.weight = PromoWeight::kHeavy;
      break;
    case FeaturePromoSpecification::PromoType::kUnspecified:
      NOTREACHED();
  }
  return promo_info;
}

FeaturePromoResult FeaturePromoSessionPolicyV2::CanShowPromo(
    PromoInfo to_show,
    std::optional<PromoInfo> currently_showing) const {
  const auto initial_result =
      FeaturePromoSessionPolicy::CanShowPromo(to_show, currently_showing);
  if (!initial_result) {
    return initial_result;
  }

  if (to_show.priority == PromoPriority::kLow &&
      to_show.weight == PromoWeight::kHeavy) {
    // Ensure session state is current since there could be a new session.
    session_manager()->MaybeUpdateSessionState();

    // TODO(dfried): Sanity check to determine if our assumption that promos
    // will not be shown in an idle browser is valid.

    const auto now = storage_service()->GetCurrentTime();
    const auto since_session_start =
        now - storage_service()->ReadSessionData().start_time;
    if (since_session_start < session_start_grace_period_) {
      return FeaturePromoResult::kBlockedByGracePeriod;
    }
    const auto since_last_promo =
        now - storage_service()->ReadPolicyData().last_heavyweight_promo_time;
    if (since_last_promo < heavyweight_promo_cooldown_) {
      return FeaturePromoResult::kBlockedByCooldown;
    }
  }
  return FeaturePromoResult::Success();
}

}  // namespace user_education
