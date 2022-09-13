// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/metrics/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

namespace commerce::metrics {

const char kPDPStateHistogramName[] = "Commerce.PDPStateOnNavigation";

void RecordPDPStateToUma(ShoppingPDPState state) {
  base::UmaHistogramEnumeration(kPDPStateHistogramName, state);
}

ShoppingPDPState ComputeStateForOptGuideResult(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue ||
      !metadata.any_metadata().has_value()) {
    return ShoppingPDPState::kNotPDP;
  }

  absl::optional<PriceTrackingData> parsed_any =
      optimization_guide::ParsedAnyMetadata<PriceTrackingData>(
          metadata.any_metadata().value());

  if (!parsed_any.has_value() || !parsed_any.value().IsInitialized())
    return ShoppingPDPState::kNotPDP;

  const PriceTrackingData& price_data = parsed_any.value();

  if (price_data.has_buyable_product() &&
      price_data.buyable_product().has_product_cluster_id()) {
    return ShoppingPDPState::kIsPDPWithClusterId;
  }

  return ShoppingPDPState::kIsPDPWithoutClusterId;
}

void RecordPDPStateForNavigation(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata,
    PrefService* pref_service,
    bool is_off_the_record) {
  // If optimization guide isn't allowed to run, don't attempt to query and
  // record the metrics.
  if (!pref_service ||
      !optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          is_off_the_record, pref_service)) {
    return;
  }

  RecordPDPStateToUma(ComputeStateForOptGuideResult(decision, metadata));
}

}  // namespace commerce::metrics
