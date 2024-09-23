// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_result.h"

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

std::ostream& operator<<(std::ostream& os,
                         FeaturePromoResult::Failure failure) {
  os << "FeaturePromoResult::";
  switch (failure) {
    case FeaturePromoResult::kCanceled:
      os << "kCanceled";
      break;
    case FeaturePromoResult::kBlockedByUi:
      os << "kBlockedByUi";
      break;
    case FeaturePromoResult::kBlockedByPromo:
      os << "kBlockedByPromo";
      break;
    case FeaturePromoResult::kBlockedByConfig:
      os << "kBlockedByConfig";
      break;
    case FeaturePromoResult::kSnoozed:
      os << "kSnoozed";
      break;
    case FeaturePromoResult::kPermanentlyDismissed:
      os << "kPermanentlyDismissed";
      break;
    case FeaturePromoResult::kBlockedByContext:
      os << "kBlockedForContext";
      break;
    case FeaturePromoResult::kFeatureDisabled:
      os << "kFeatureDisabled";
      break;
    case FeaturePromoResult::kError:
      os << "kError";
      break;
    case FeaturePromoResult::kBlockedByGracePeriod:
      os << "kBlockedByGracePeriod";
      break;
    case FeaturePromoResult::kBlockedByCooldown:
      os << "kBlockedByCooldown";
      break;
    case FeaturePromoResult::kRecentlyAborted:
      os << "kRecentlyAborted";
      break;
    case FeaturePromoResult::kExceededMaxShowCount:
      os << "kExceededMaxShowCount";
      break;
    case FeaturePromoResult::kBlockedByNewProfile:
      os << "kBlockedByNewProfile";
      break;
    case FeaturePromoResult::kBlockedByReshowDelay:
      os << "kBlockedByReshowDelay";
      break;
  }
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
