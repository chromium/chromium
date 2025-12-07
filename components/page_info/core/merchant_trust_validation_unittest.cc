// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_validation.h"

#include "base/test/scoped_feature_list.h"
#include "components/page_info/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_info::merchant_trust_validation {

commerce::MerchantTrustSignalsV2 GetSampleProto() {
  commerce::MerchantTrustSignalsV2 proto;
  proto.set_merchant_star_rating(4.5);
  proto.set_merchant_count_rating(100);
  proto.set_merchant_details_page_url("https://example.com");
  proto.set_shopper_voice_summary("Great product");
  return proto;
}

TEST(MerchantTrustValidation, NoResult) {
  EXPECT_EQ(ValidateProto(std::nullopt), MerchantTrustStatus::kNoResult);
}

// Tests that correct proto messages are accepted.
TEST(MerchantTrustValidation, ValidProto) {
  auto proto = GetSampleProto();
  EXPECT_EQ(ValidateProto(proto), MerchantTrustStatus::kValid);
}

TEST(MerchantTrustValidation, MissingStarRating) {
  auto proto = GetSampleProto();
  proto.clear_merchant_star_rating();
  EXPECT_EQ(ValidateProto(proto), MerchantTrustStatus::kMissingRatingValue);
}

TEST(MerchantTrustValidation, MissingCountRating) {
  auto proto = GetSampleProto();
  proto.clear_merchant_count_rating();
  EXPECT_EQ(ValidateProto(proto), MerchantTrustStatus::kMissingRatingCount);
}

TEST(MerchantTrustValidation, MissingReviewsSummary) {
  auto proto = GetSampleProto();
  proto.clear_shopper_voice_summary();
  EXPECT_EQ(ValidateProto(proto),
            MerchantTrustStatus::kValidWithMissingReviewsSummary);
}

TEST(MerchantTrustValidation, MissingReviewsPageUrl) {
  auto proto = GetSampleProto();
  proto.clear_merchant_details_page_url();
  EXPECT_EQ(ValidateProto(proto), MerchantTrustStatus::kMissingReviewsPageUrl);
}

TEST(MerchantTrustValidation, InvalidReviewsPageUrl) {
  auto proto = GetSampleProto();
  proto.set_merchant_details_page_url("not a url");
  EXPECT_EQ(ValidateProto(proto), MerchantTrustStatus::kInvalidReviewsPageUrl);
}

}  // namespace page_info::merchant_trust_validation
