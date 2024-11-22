// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <optional>

#include "base/feature_list.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/merchant_trust_metadata.pb.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace page_info {

MerchantTrustService::MerchantTrustService(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    bool is_off_the_record,
    PrefService* prefs)
    : optimization_guide_decider_(optimization_guide_decider),
      is_off_the_record_(is_off_the_record),
      prefs_(prefs) {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V3});
  }
}

std::optional<page_info::proto::MerchantTrustSignalsV3>
MerchantTrustService::GetMerchantTrustInfo(const GURL& url,
                                           ukm::SourceId source_id) const {
  if (!optimization_guide::IsValidURLForURLKeyedHint(url)) {
    return std::nullopt;
  }

  if (!IsOptimizationGuideAllowed()) {
    return std::nullopt;
  }
  // TODO(crbug.com/378671877): Returning a stubbed response for now.
  page_info::proto::MerchantTrustSignalsV3 merchant_trust_signals;
  merchant_trust_signals.set_star_rating(4.5);
  merchant_trust_signals.set_count_rating(100);
  merchant_trust_signals.set_page_url(
      "https://customerreviews.google.com/v/merchant?q=amazon.com&c=AE&v=19");
  merchant_trust_signals.set_overall_summary(
      "This is a test summary for the merchant trust side panel.");

  // TODO(crbug.com/378671877): Call the optimization guide decider with the new
  // optimization type.

  // TODO(crbug.com/378671877): Add and log validation for
  // MerchantTrustSignalsV3.

  return merchant_trust_signals;
}

MerchantTrustService::~MerchantTrustService() = default;

bool MerchantTrustService::IsOptimizationGuideAllowed() const {
  return optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
      is_off_the_record_, prefs_);
}

}  // namespace page_info
