// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_STORAGE_SERVICE_H_

#include <map>
#include <optional>

#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_storage_service.h"

namespace user_education::test {

// Version of FeaturePromoStorageService that stores data in an in-memory map
// for testing.
class TestFeaturePromoStorageService : public FeaturePromoStorageService {
 public:
  TestFeaturePromoStorageService();
  ~TestFeaturePromoStorageService() override;

  // FeaturePromoStorageService:
  std::optional<FeaturePromoData> ReadPromoData(
      const base::Feature& iph_feature) const override;
  void SavePromoData(const base::Feature& iph_feature,
                     const FeaturePromoData& promo_data) override;
  void Reset(const base::Feature& iph_feature) override;
  FeaturePromoSessionData ReadSessionData() const override;
  void SaveSessionData(const FeaturePromoSessionData& session_data) override;
  void ResetSession() override;
  FeaturePromoPolicyData ReadPolicyData() const override;
  void SavePolicyData(const FeaturePromoPolicyData& policy_data) override;
  void ResetPolicy() override;
  user_education::NewBadgeData ReadNewBadgeData(
      const base::Feature& new_badge_feature) const override;
  void SaveNewBadgeData(const base::Feature& new_badge_feature,
                        const NewBadgeData& new_badge_data) override;
  void ResetNewBadge(const base::Feature& new_badge_feature) override;

 private:
  std::map<const base::Feature*, FeaturePromoData> promo_data_;
  FeaturePromoSessionData session_data_;
  FeaturePromoPolicyData policy_data_;
  std::map<const base::Feature*, NewBadgeData> new_badge_data_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_STORAGE_SERVICE_H_
