// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_feature_promo_storage_service.h"

#include "components/user_education/common/feature_promo_data.h"

namespace user_education::test {

TestFeaturePromoStorageService::TestFeaturePromoStorageService() = default;
TestFeaturePromoStorageService::~TestFeaturePromoStorageService() = default;

std::optional<FeaturePromoData> TestFeaturePromoStorageService::ReadPromoData(
    const base::Feature& iph_feature) const {
  const auto it = promo_data_.find(&iph_feature);
  return it == promo_data_.end() ? std::nullopt
                                 : std::make_optional(it->second);
}

void TestFeaturePromoStorageService::SavePromoData(
    const base::Feature& iph_feature,
    const FeaturePromoData& promo_data) {
  promo_data_[&iph_feature] = promo_data;
}

void TestFeaturePromoStorageService::Reset(const base::Feature& iph_feature) {
  promo_data_.erase(&iph_feature);
}

FeaturePromoSessionData TestFeaturePromoStorageService::ReadSessionData()
    const {
  return session_data_;
}

void TestFeaturePromoStorageService::SaveSessionData(
    const FeaturePromoSessionData& session_data) {
  session_data_ = session_data;
}

void TestFeaturePromoStorageService::ResetSession() {
  session_data_ = FeaturePromoSessionData();
}

FeaturePromoPolicyData TestFeaturePromoStorageService::ReadPolicyData() const {
  return policy_data_;
}

void TestFeaturePromoStorageService::SavePolicyData(
    const FeaturePromoPolicyData& policy_data) {
  policy_data_ = policy_data;
}

void TestFeaturePromoStorageService::ResetPolicy() {
  policy_data_ = FeaturePromoPolicyData();
}

NewBadgeData TestFeaturePromoStorageService::ReadNewBadgeData(
    const base::Feature& new_badge_feature) const {
  const auto it = new_badge_data_.find(&new_badge_feature);
  return it == new_badge_data_.end() ? NewBadgeData() : it->second;
}

void TestFeaturePromoStorageService::SaveNewBadgeData(
    const base::Feature& new_badge_feature,
    const NewBadgeData& new_badge_data) {
  new_badge_data_[&new_badge_feature] = new_badge_data;
}

void TestFeaturePromoStorageService::ResetNewBadge(
    const base::Feature& new_badge_feature) {
  new_badge_data_.erase(&new_badge_feature);
}

ProductMessagingData TestFeaturePromoStorageService::ReadProductMessagingData()
    const {
  return product_messaging_data_;
}

void TestFeaturePromoStorageService::SaveProductMessagingData(
    const ProductMessagingData& product_messaging_data) {
  product_messaging_data_ = product_messaging_data;
}

void TestFeaturePromoStorageService::ResetProductMessagingData() {
  product_messaging_data_ = ProductMessagingData();
}

}  // namespace user_education::test
