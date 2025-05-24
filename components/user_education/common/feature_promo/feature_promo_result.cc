// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_result.h"

#include <array>

namespace user_education {

FeaturePromoResult::FeaturePromoResult(const FeaturePromoResult& other) =
    default;

FeaturePromoResult& FeaturePromoResult::operator=(
    const FeaturePromoResult& other) = default;

FeaturePromoResult& FeaturePromoResult::operator=(Failure failure) {
  failure_ = failure;
  return *this;
}

FeaturePromoResult::~FeaturePromoResult() = default;

// static
FeaturePromoResult FeaturePromoResult::Success() {
  return FeaturePromoResult();
}

// static
std::string FeaturePromoResult::GetFailureName(Failure failure) {
  // LINT.IfChange(failure_names)
  constexpr static std::array<const char*, Failure::kMaxValue + 1>
      kFailureNames{"Canceled",
                    "Error",
                    "BlockedByUi",
                    "BlockedByPromo",
                    "BlockedByConfig",
                    "Snoozed",
                    "BlockedByContext",
                    "FeatureDisabled",
                    "PermanentlyDismissed",
                    "BlockedByGracePeriod",
                    "BlockedByCooldown",
                    "RecentlyAborted",
                    "ExceededMaxShowCount",
                    "BlockedByNewProfile",
                    "BlockedByReshowDelay",
                    "TimedOut",
                    "AlreadyQueued",
                    "AnchorNotVisible",
                    "AnchorSurfaceNotActive",
                    "WindowTooSmall",
                    "BlockedByUserActivity"};
  // LINT.ThenChange(//components/user_education/common/feature_promo/feature_promo_result.h:failure_enum)
  return kFailureNames[failure];
}

std::ostream& operator<<(std::ostream& os,
                         FeaturePromoResult::Failure failure) {
  os << "FeaturePromoResult::k" << FeaturePromoResult::GetFailureName(failure);
  return os;
}

std::ostream& operator<<(std::ostream& os, const FeaturePromoResult& result) {
  if (!result) {
    os << *result.failure();
  } else {
    os << "FeaturePromoResult::Success()";
  }
  return os;
}

}  // namespace user_education
