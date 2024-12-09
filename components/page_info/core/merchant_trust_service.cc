// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/page_info_types.h"
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
      prefs_(prefs),
      weak_ptr_factory_(this) {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V2});
  }
}

void MerchantTrustService::GetMerchantTrustInfo(
    const GURL& url,
    MerchantDataCallback callback) const {
  if (!optimization_guide::IsValidURLForURLKeyedHint(url)) {
    std::move(callback).Run(url, std::nullopt);
    return;
  }

  if (!IsOptimizationGuideAllowed()) {
    std::move(callback).Run(url, std::nullopt);
    return;
  }

  optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V2,
      base::BindOnce(&MerchantTrustService::OnCanApplyOptimizationComplete,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void MerchantTrustService::OnCanApplyOptimizationComplete(
    const GURL& url,
    MerchantDataCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) const {
  if (decision != optimization_guide::OptimizationGuideDecision::kUnknown) {
    // TODO(tommasin): Add and log validation for
    // the proto.
    std::optional<commerce::MerchantTrustSignalsV2> merchant_trust_metadata =
        metadata.ParsedMetadata<commerce::MerchantTrustSignalsV2>();
    if (merchant_trust_metadata.has_value()) {
      std::move(callback).Run(
          url, GetMerchantDataFromProto(merchant_trust_metadata));
      return;
    }
  }

  if (kMerchantTrustEnabledWithSampleData.Get()) {
    std::move(callback).Run(url, GetSampleData());
    return;
  }
  std::move(callback).Run(url, std::nullopt);
}

MerchantTrustService::~MerchantTrustService() = default;

bool MerchantTrustService::IsOptimizationGuideAllowed() const {
  return optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
      is_off_the_record_, prefs_);
}

std::optional<page_info::MerchantData>
MerchantTrustService::GetMerchantDataFromProto(
    const std::optional<commerce::MerchantTrustSignalsV2>& metadata) const {
  if (!metadata.has_value()) {
    return std::nullopt;
  }

  std::optional<page_info::MerchantData> merchant_data;

  commerce::MerchantTrustSignalsV2 merchant_proto = metadata.value();
  if (metadata.has_value() && merchant_proto.IsInitialized()) {
    merchant_data.emplace();

    if (merchant_proto.has_merchant_star_rating()) {
      merchant_data->star_rating = merchant_proto.merchant_star_rating();
    }

    if (merchant_proto.has_merchant_count_rating()) {
      merchant_data->count_rating = merchant_proto.merchant_count_rating();
    }

    if (merchant_proto.has_merchant_details_page_url()) {
      GURL page_url = GURL(merchant_proto.merchant_details_page_url());
      if (page_url.is_valid()) {
        merchant_data->page_url = page_url;
      }
    }

    if (merchant_proto.has_reviews_summary()) {
      merchant_data->reviews_summary = merchant_proto.reviews_summary();
    }
  }

  return merchant_data;
}

}  // namespace page_info
