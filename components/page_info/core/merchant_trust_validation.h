// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_VALIDATION_H_
#define COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_VALIDATION_H_

#include <optional>

#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/page_info/core/page_info_types.h"

namespace page_info::merchant_trust_validation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with MerchantTrustStatus in enums.xml

// LINT.IfChange(MerchantTrustStatus)
enum class MerchantTrustStatus {
  kValid = 0,
  kNoResult = 1,
  kMissingRatingCount = 2,
  kMissingRatingValue = 3,
  // kMissingReviewsSummary = 4,  Deprecated.
  kMissingReviewsPageUrl = 5,
  kInvalidReviewsPageUrl = 6,
  kValidWithMissingReviewsSummary = 7,

  kMaxValue = kValidWithMissingReviewsSummary,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/security/enums.xml:MerchantTrustStatus)

MerchantTrustStatus ValidateProto(
    const std::optional<commerce::MerchantTrustSignalsV2>& merchant_proto);

}  // namespace page_info::merchant_trust_validation

#endif  // COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_VALIDATION_H_
