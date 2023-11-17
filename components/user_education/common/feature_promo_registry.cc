// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_registry.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_specification.h"

namespace user_education {

FeaturePromoRegistry::FeaturePromoRegistry() = default;
FeaturePromoRegistry::FeaturePromoRegistry(
    FeaturePromoRegistry&& other) noexcept = default;
FeaturePromoRegistry& FeaturePromoRegistry::operator=(
    FeaturePromoRegistry&& other) noexcept = default;
FeaturePromoRegistry::~FeaturePromoRegistry() = default;

bool FeaturePromoRegistry::IsFeatureRegistered(
    const base::Feature& iph_feature) const {
  return base::Contains(feature_promo_data_, &iph_feature);
}

const FeaturePromoSpecification* FeaturePromoRegistry::GetParamsForFeature(
    const base::Feature& iph_feature) const {
  auto data_it = feature_promo_data_.find(&iph_feature);
  DCHECK(data_it != feature_promo_data_.end());
  return &data_it->second;
}

void FeaturePromoRegistry::RegisterFeature(FeaturePromoSpecification spec) {
  const base::Feature* const iph_feature = spec.feature();
  CHECK(iph_feature);
  const auto result = feature_promo_data_.emplace(iph_feature, std::move(spec));
  DCHECK(result.second) << "Duplicate IPH feature registered: "
                        << iph_feature->name;
}

void FeaturePromoRegistry::ClearFeaturesForTesting() {
  feature_promo_data_.clear();
}

}  // namespace user_education
