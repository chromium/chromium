// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_feature_promo_storage_service.h"

namespace user_education::test {

TestFeaturePromoStorageService::TestFeaturePromoStorageService() = default;
TestFeaturePromoStorageService::~TestFeaturePromoStorageService() = default;

absl::optional<TestFeaturePromoStorageService::PromoData>
TestFeaturePromoStorageService::ReadPromoData(
    const base::Feature& iph_feature) const {
  const auto it = promo_data_.find(&iph_feature);
  return it == promo_data_.end() ? absl::nullopt
                                 : absl::make_optional(it->second);
}

void TestFeaturePromoStorageService::SavePromoData(
    const base::Feature& iph_feature,
    const PromoData& promo_data) {
  promo_data_[&iph_feature] = promo_data;
}

void TestFeaturePromoStorageService::Reset(const base::Feature& iph_feature) {
  promo_data_.erase(&iph_feature);
}

}  // namespace user_education::test
