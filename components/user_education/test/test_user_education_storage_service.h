// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_USER_EDUCATION_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_USER_EDUCATION_STORAGE_SERVICE_H_

#include <map>
#include <optional>

#include "base/feature_list.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education::test {

// Version of UserEducationStorageService that stores data in an in-memory map
// for testing.
class TestUserEducationStorageService : public UserEducationStorageService {
 public:
  TestUserEducationStorageService();
  ~TestUserEducationStorageService() override;

  // UserEducationStorageService:
  std::optional<FeaturePromoData> ReadPromoData(
      const base::Feature& iph_feature) const override;
  void SavePromoData(const base::Feature& iph_feature,
                     const FeaturePromoData& promo_data) override;
  void Reset(const base::Feature& iph_feature) override;
  UserEducationSessionData ReadSessionData() const override;
  void SaveSessionData(const UserEducationSessionData& session_data) override;
  void ResetSession() override;
  FeaturePromoPolicyData ReadPolicyData() const override;
  void SavePolicyData(const FeaturePromoPolicyData& policy_data) override;
  void ResetPolicy() override;
  NewBadgeData ReadNewBadgeData(
      const base::Feature& new_badge_feature) const override;
  void SaveNewBadgeData(const base::Feature& new_badge_feature,
                        const NewBadgeData& new_badge_data) override;
  void ResetNewBadge(const base::Feature& new_badge_feature) override;
  ProductMessagingData ReadProductMessagingData() const override;
  void SaveProductMessagingData(
      const ProductMessagingData& product_messaging_data) override;
  void ResetProductMessagingData() override;

 private:
  std::map<const base::Feature*, FeaturePromoData> promo_data_;
  UserEducationSessionData session_data_;
  FeaturePromoPolicyData policy_data_;
  std::map<const base::Feature*, NewBadgeData> new_badge_data_;
  ProductMessagingData product_messaging_data_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_USER_EDUCATION_STORAGE_SERVICE_H_
