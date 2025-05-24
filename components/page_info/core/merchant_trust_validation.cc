// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_validation.h"

#include "base/feature_list.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/page_info_types.h"
#include "url/gurl.h"

namespace page_info::merchant_trust_validation {

MerchantTrustStatus ValidateProto(
    const std::optional<commerce::MerchantTrustSignalsV2>& merchant_proto) {
  if (!merchant_proto.has_value()) {
    return MerchantTrustStatus::kNoResult;
  }
  if (!merchant_proto->has_merchant_count_rating()) {
    return MerchantTrustStatus::kMissingRatingCount;
  }
  if (!merchant_proto->has_merchant_star_rating()) {
    return MerchantTrustStatus::kMissingRatingValue;
  }
  if (!merchant_proto->has_merchant_details_page_url()) {
    return MerchantTrustStatus::kMissingReviewsPageUrl;
  }
  if (!GURL(merchant_proto->merchant_details_page_url()).is_valid()) {
    return MerchantTrustStatus::kInvalidReviewsPageUrl;
  }
  if (!merchant_proto->has_shopper_voice_summary()) {
    return MerchantTrustStatus::kValidWithMissingReviewsSummary;
  }

  return MerchantTrustStatus::kValid;
}

}  // namespace page_info::merchant_trust_validation
