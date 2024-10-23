// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_user_education_storage_service.h"

#include "components/user_education/common/user_education_data.h"

namespace user_education::test {

TestUserEducationStorageService::TestUserEducationStorageService() = default;
TestUserEducationStorageService::~TestUserEducationStorageService() = default;

std::optional<FeaturePromoData> TestUserEducationStorageService::ReadPromoData(
    const base::Feature& iph_feature) const {
  const auto it = promo_data_.find(&iph_feature);
  return it == promo_data_.end() ? std::nullopt
                                 : std::make_optional(it->second);
}

void TestUserEducationStorageService::SavePromoData(
    const base::Feature& iph_feature,
    const FeaturePromoData& promo_data) {
  promo_data_[&iph_feature] = promo_data;
}

void TestUserEducationStorageService::Reset(const base::Feature& iph_feature) {
  promo_data_.erase(&iph_feature);
}

UserEducationSessionData TestUserEducationStorageService::ReadSessionData()
    const {
  return session_data_;
}

void TestUserEducationStorageService::SaveSessionData(
    const UserEducationSessionData& session_data) {
  session_data_ = session_data;
}

void TestUserEducationStorageService::ResetSession() {
  session_data_ = UserEducationSessionData();
}

FeaturePromoPolicyData TestUserEducationStorageService::ReadPolicyData() const {
  return policy_data_;
}

void TestUserEducationStorageService::SavePolicyData(
    const FeaturePromoPolicyData& policy_data) {
  policy_data_ = policy_data;
}

void TestUserEducationStorageService::ResetPolicy() {
  policy_data_ = FeaturePromoPolicyData();
}

NewBadgeData TestUserEducationStorageService::ReadNewBadgeData(
    const base::Feature& new_badge_feature) const {
  const auto it = new_badge_data_.find(&new_badge_feature);
  return it == new_badge_data_.end() ? NewBadgeData() : it->second;
}

void TestUserEducationStorageService::SaveNewBadgeData(
    const base::Feature& new_badge_feature,
    const NewBadgeData& new_badge_data) {
  new_badge_data_[&new_badge_feature] = new_badge_data;
}

void TestUserEducationStorageService::ResetNewBadge(
    const base::Feature& new_badge_feature) {
  new_badge_data_.erase(&new_badge_feature);
}

ProductMessagingData TestUserEducationStorageService::ReadProductMessagingData()
    const {
  return product_messaging_data_;
}

void TestUserEducationStorageService::SaveProductMessagingData(
    const ProductMessagingData& product_messaging_data) {
  product_messaging_data_ = product_messaging_data;
}

void TestUserEducationStorageService::ResetProductMessagingData() {
  product_messaging_data_ = ProductMessagingData();
}

}  // namespace user_education::test
