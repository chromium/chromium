// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <optional>

#include "base/feature_list.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/page_info_types.h"
#include "components/page_info/core/proto/merchant_trust_metadata.pb.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace page_info {
using OptimizationGuideDecision = optimization_guide::OptimizationGuideDecision;

namespace {

std::optional<page_info::MerchantData> GetSampleData() {
  page_info::MerchantData merchant_data;

  merchant_data.star_rating = 4.5;
  merchant_data.count_rating = 100;
  merchant_data.page_url = GURL(
      "https://customerreviews.google.com/v/merchant?q=amazon.com&c=AE&v=19");
  merchant_data.reviews_summary =
      "This is a test summary for the merchant trust side panel.";
  return merchant_data;
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

std::optional<page_info::MerchantData>
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

  if (decision != optimization_guide::OptimizationGuideDecision::kUnknown) {
    // TODO(tommasin): Add and log validation for
    // the proto.
    return GetMerchantDataFromProto(merchant_trust_metadata);
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

std::optional<page_info::MerchantData>
MerchantTrustService::GetMerchantDataFromProto(
    const std::optional<proto::MerchantTrustSignalsV3>& metadata) const {
  if (!metadata.has_value()) {
    return std::nullopt;
  }

  std::optional<page_info::MerchantData> merchant_data;

  page_info::proto::MerchantTrustSignalsV3 merchant_proto = metadata.value();
  if (metadata.has_value() && merchant_proto.IsInitialized()) {
    merchant_data.emplace();

    if (merchant_proto.has_star_rating()) {
      merchant_data->star_rating = merchant_proto.star_rating();
    }

    if (merchant_proto.has_count_rating()) {
      merchant_data->count_rating = merchant_proto.count_rating();
    }

    if (merchant_proto.has_page_url()) {
      GURL page_url = GURL(merchant_proto.page_url());
      if (page_url.is_valid()) {
        merchant_data->page_url = page_url;
      }
    }

    if (merchant_proto.has_overall_summary()) {
      merchant_data->reviews_summary = merchant_proto.overall_summary();
    }
  }

  return merchant_data;
}

}  // namespace page_info
