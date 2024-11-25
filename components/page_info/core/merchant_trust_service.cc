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
using OptimizationGuideDecision = optimization_guide::OptimizationGuideDecision;

namespace {

std::optional<page_info::proto::MerchantTrustSignalsV3> GetSampleData() {
  page_info::proto::MerchantTrustSignalsV3 merchant_trust_signals;

  merchant_trust_signals.set_star_rating(4.5);
  merchant_trust_signals.set_count_rating(100);
  merchant_trust_signals.set_page_url(
      "https://customerreviews.google.com/v/merchant?q=amazon.com&c=AE&v=19");
  merchant_trust_signals.set_overall_summary(
      "This is a test summary for the merchant trust side panel.");
  return merchant_trust_signals;
}
}  // namespace

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

  std::optional<proto::MerchantTrustSignalsV3> merchant_trust_metadata;
  optimization_guide::OptimizationGuideDecision decision;
  optimization_guide::OptimizationMetadata metadata;
  decision = CanApplyOptimization(url, &metadata);
  merchant_trust_metadata =
      metadata.ParsedMetadata<proto::MerchantTrustSignalsV3>();

  if(decision != optimization_guide::OptimizationGuideDecision::kUnknown) {
    // TODO(tommasin): Add and log validation for
    // MerchantTrustSignalsV3.
    return merchant_trust_metadata;
  }

  if (kMerchantTrustEnabledWithSampleData.Get()) {
    return GetSampleData();
  }

  return std::nullopt;
}

MerchantTrustService::~MerchantTrustService() = default;

bool MerchantTrustService::IsOptimizationGuideAllowed() const {
  return optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
      is_off_the_record_, prefs_);
}

optimization_guide::OptimizationGuideDecision
MerchantTrustService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::OptimizationMetadata* optimization_metadata) const {
  if (!IsOptimizationGuideAllowed()) {
    return optimization_guide::OptimizationGuideDecision::kUnknown;
  }
  return optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V3,
      optimization_metadata);
}

}  // namespace page_info
