// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_data.h"

namespace user_education {

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoClosedReason close_reason) {
  switch (close_reason) {
    case FeaturePromoClosedReason::kDismiss:
      oss << "kDismiss";
      break;
    case FeaturePromoClosedReason::kSnooze:
      oss << "kSnooze";
      break;
    case FeaturePromoClosedReason::kAction:
      oss << "kAction";
      break;
    case FeaturePromoClosedReason::kCancel:
      oss << "kCancel";
      break;
    case FeaturePromoClosedReason::kTimeout:
      oss << "kTimeout";
      break;
    case FeaturePromoClosedReason::kAbortPromo:
      oss << "kAbortPromo";
      break;
    case FeaturePromoClosedReason::kFeatureEngaged:
      oss << "kFeatureEngaged";
      break;
    case FeaturePromoClosedReason::kOverrideForUIRegionConflict:
      oss << "kOverrideForUIRegionConflict";
      break;
    case FeaturePromoClosedReason::kOverrideForDemo:
      oss << "kOverrideForDemo";
      break;
    case FeaturePromoClosedReason::kOverrideForTesting:
      oss << "kOverrideForTesting";
      break;
    case FeaturePromoClosedReason::kOverrideForPrecedence:
      oss << "kOverrideForPrecedence";
      break;
  }
  return oss;
}

FeaturePromoData::FeaturePromoData() = default;
FeaturePromoData::~FeaturePromoData() = default;
FeaturePromoData::FeaturePromoData(const FeaturePromoData&) = default;
FeaturePromoData::FeaturePromoData(FeaturePromoData&&) = default;
FeaturePromoData& FeaturePromoData::operator=(const FeaturePromoData&) =
    default;
FeaturePromoData& FeaturePromoData::operator=(FeaturePromoData&&) = default;

}  // namespace user_education
