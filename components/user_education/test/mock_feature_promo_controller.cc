// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/mock_feature_promo_controller.h"

namespace user_education::test {

MockFeaturePromoController::MockFeaturePromoController() = default;
MockFeaturePromoController::~MockFeaturePromoController() = default;

base::WeakPtr<FeaturePromoController>
MockFeaturePromoController::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

FeaturePromoParamsMatcher::FeaturePromoParamsMatcher(
    const base::Feature& feature)
    : feature_(feature) {}
FeaturePromoParamsMatcher::FeaturePromoParamsMatcher(
    const FeaturePromoParamsMatcher&) = default;
FeaturePromoParamsMatcher::~FeaturePromoParamsMatcher() = default;
FeaturePromoParamsMatcher& FeaturePromoParamsMatcher::operator=(
    const FeaturePromoParamsMatcher&) = default;

bool FeaturePromoParamsMatcher::MatchAndExplain(
    const FeaturePromoParams& params,
    std::ostream*) const {
  return &params.feature.get() == &feature_.get();
}

void FeaturePromoParamsMatcher::DescribeTo(std::ostream* os) const {
  *os << "FeaturePromoParams has feature " << feature_->name;
}

void FeaturePromoParamsMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "FeaturePromoParams does not have feature " << feature_->name;
}

}  // namespace user_education::test
