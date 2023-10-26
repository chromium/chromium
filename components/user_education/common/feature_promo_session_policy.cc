// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_policy.h"

#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"

namespace user_education {

FeaturePromoSessionPolicy::FeaturePromoSessionPolicy() = default;
FeaturePromoSessionPolicy::~FeaturePromoSessionPolicy() = default;

FeaturePromoSessionPolicyV1::FeaturePromoSessionPolicyV1() = default;
FeaturePromoSessionPolicyV1::~FeaturePromoSessionPolicyV1() = default;

FeaturePromoResult FeaturePromoSessionPolicyV1::CanShowPromo(
    const FeaturePromoSpecification& promo_specification) const {
  return FeaturePromoResult::Success();
}

void FeaturePromoSessionPolicyV1::OnPromoShown(
    const FeaturePromoSpecification& promo_specification) {
  // No-op.
}

FeaturePromoSessionPolicyV2::FeaturePromoSessionPolicyV2() = default;
FeaturePromoSessionPolicyV2::~FeaturePromoSessionPolicyV2() = default;

FeaturePromoResult FeaturePromoSessionPolicyV2::CanShowPromo(
    const FeaturePromoSpecification& promo_specification) const {
  return FeaturePromoResult::Success();
}

void FeaturePromoSessionPolicyV2::OnPromoShown(
    const FeaturePromoSpecification& promo_specification) {
  // No-op.
}

}  // namespace user_education
