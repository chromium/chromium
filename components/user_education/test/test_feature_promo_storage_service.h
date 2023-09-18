// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_STORAGE_SERVICE_H_

#include <map>

#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_education::test {

// Version of FeaturePromoStorageService that stores data in an in-memory map
// for testing.
class TestFeaturePromoStorageService : public FeaturePromoStorageService {
 public:
  TestFeaturePromoStorageService();
  ~TestFeaturePromoStorageService() override;

  // FeaturePromoStorageService:
  absl::optional<PromoData> ReadPromoData(
      const base::Feature& iph_feature) const override;
  void SavePromoData(const base::Feature& iph_feature,
                     const PromoData& promo_data) override;
  void Reset(const base::Feature& iph_feature) override;

 private:
  std::map<const base::Feature*, PromoData> promo_data_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_STORAGE_SERVICE_H_
