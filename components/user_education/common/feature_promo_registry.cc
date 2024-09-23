// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_registry.h"

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/new_badge_specification.h"

namespace user_education {

FeaturePromoRegistry::FeaturePromoRegistry() = default;
FeaturePromoRegistry::FeaturePromoRegistry(
    FeaturePromoRegistry&& other) noexcept = default;
FeaturePromoRegistry& FeaturePromoRegistry::operator=(
    FeaturePromoRegistry&& other) noexcept = default;
FeaturePromoRegistry::~FeaturePromoRegistry() = default;

void FeaturePromoRegistry::RegisterFeature(FeaturePromoSpecification spec) {
  const base::Feature* const iph_feature = spec.feature();
  CHECK(iph_feature);
  CHECK_NE(FeaturePromoSpecification::PromoType::kUnspecified,
           spec.promo_type());
  FeatureRegistry<FeaturePromoSpecification>::RegisterFeature(*iph_feature,
                                                              std::move(spec));
}

NewBadgeRegistry::NewBadgeRegistry() = default;
NewBadgeRegistry::NewBadgeRegistry(NewBadgeRegistry&& other) noexcept = default;
NewBadgeRegistry& NewBadgeRegistry::operator=(
    NewBadgeRegistry&& other) noexcept = default;
NewBadgeRegistry::~NewBadgeRegistry() = default;

void NewBadgeRegistry::RegisterFeature(NewBadgeSpecification spec) {
  const base::Feature* const iph_feature = spec.feature;
  CHECK(iph_feature);
  FeatureRegistry<NewBadgeSpecification>::RegisterFeature(*iph_feature,
                                                          std::move(spec));
}

}  // namespace user_education
