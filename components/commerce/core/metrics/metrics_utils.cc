// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/metrics/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace commerce::metrics {

const char kPDPNavShoppingListEligibleHistogramName[] =
    "Commerce.PDPNavigation.ShoppingList.Eligible";
const char kPDPStateHistogramName[] = "Commerce.PDPStateOnNavigation";
const char kPDPStateWithLocalMetaName[] = "Commerce.PDPStateWithLocalMeta";
const char kShoppingListIneligibleHistogramName[] =
    "Commerce.PDPNavigation.ShoppingList.IneligibilityReason";
const char kPDPNavURLSize[] = "Commerce.PDPNavigation.URLSize";

void RecordPDPStateToUma(ShoppingPDPState state) {
  base::UmaHistogramEnumeration(kPDPStateHistogramName, state);
}

void RecordPDPNavShoppingListEligible(ShoppingPDPState state,
                                      bool is_shopping_list_eligible) {
  // Only record this metric for pages that were determined to be PDPs.
  if (state == ShoppingPDPState::kNotPDP) {
    return;
  }

  base::UmaHistogramBoolean(kPDPNavShoppingListEligibleHistogramName,
                            is_shopping_list_eligible);
}

void RecordUrlSizeforPDP(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata,
    const GURL& url) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue ||
      !metadata.any_metadata().has_value()) {
    return;
  }
  base::UmaHistogramCounts10000(kPDPNavURLSize, url.spec().size());
}

ShoppingPDPState ComputeStateForOptGuideResult(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue ||
      !metadata.any_metadata().has_value()) {
    return ShoppingPDPState::kNotPDP;
  }

  std::optional<PriceTrackingData> parsed_any =
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

void RecordPDPMetrics(optimization_guide::OptimizationGuideDecision decision,
                      const optimization_guide::OptimizationMetadata& metadata,
                      PrefService* pref_service,
                      bool is_off_the_record,
                      bool is_shopping_list_eligible,
                      const GURL& url) {
  // If optimization guide isn't allowed to run, don't attempt to query and
  // record the metrics.
  if (!pref_service ||
      !optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          is_off_the_record, pref_service)) {
    return;
  }

  ShoppingPDPState state = ComputeStateForOptGuideResult(decision, metadata);

  RecordPDPStateToUma(state);
  RecordPDPNavShoppingListEligible(state, is_shopping_list_eligible);
  RecordUrlSizeforPDP(decision, metadata, url);
}

void RecordPDPStateWithLocalMeta(bool detected_by_server,
                                 bool detected_by_client,
                                 ukm::SourceId source_id) {
  ShoppingPDPDetectionMethod detection_method =
      ShoppingPDPDetectionMethod::kNotPDP;
  if (detected_by_server && detected_by_client) {
    detection_method = ShoppingPDPDetectionMethod::kPDPServerAndLocalMeta;
  } else if (detected_by_server) {
    detection_method = ShoppingPDPDetectionMethod::kPDPServerOnly;
  } else if (detected_by_client) {
    detection_method = ShoppingPDPDetectionMethod::kPDPLocalMetaOnly;
  }

  base::UmaHistogramEnumeration(kPDPStateWithLocalMetaName, detection_method);

  ukm::builders::Shopping_PDPStateWithLocalInfo(source_id)
      .SetPDPState(static_cast<int64_t>(detection_method))
      .Record(ukm::UkmRecorder::Get());
}

void RecordShoppingListIneligibilityReasons(
    PrefService* pref_service,
    commerce::AccountChecker* account_checker,
    bool is_off_the_record,
    bool supported_country) {
  if (!supported_country) {
    base::UmaHistogramEnumeration(
        kShoppingListIneligibleHistogramName,
        ShoppingFeatureIneligibilityReason::kUnsupportedCountryOrLocale);
  }

  if (!IsShoppingListAllowedForEnterprise(pref_service)) {
    base::UmaHistogramEnumeration(
        kShoppingListIneligibleHistogramName,
        ShoppingFeatureIneligibilityReason::kEnterprisePolicy);
  }

  if (!account_checker->IsSignedIn()) {
    base::UmaHistogramEnumeration(kShoppingListIneligibleHistogramName,
                                  ShoppingFeatureIneligibilityReason::kSignin);
  }

  if (!account_checker->IsSyncingBookmarks()) {
    base::UmaHistogramEnumeration(kShoppingListIneligibleHistogramName,
                                  ShoppingFeatureIneligibilityReason::kSync);
  }

  if (!account_checker->IsAnonymizedUrlDataCollectionEnabled()) {
    base::UmaHistogramEnumeration(kShoppingListIneligibleHistogramName,
                                  ShoppingFeatureIneligibilityReason::kMSBB);
  }

  if (account_checker->IsSubjectToParentalControls()) {
    base::UmaHistogramEnumeration(
        kShoppingListIneligibleHistogramName,
        ShoppingFeatureIneligibilityReason::kParentalControls);
  }
}

void RecordShoppingActionUKM(ukm::SourceId ukm_source_id,
                             ShoppingAction action) {
  auto ukm_builder = ukm::builders::Shopping_ShoppingAction(ukm_source_id);
  switch (action) {
    case ShoppingAction::kDiscountCopied:
      ukm_builder.SetDiscountCopied(true);
      break;
    case ShoppingAction::kDiscountOpened:
      ukm_builder.SetDiscountOpened(true);
      break;
    case ShoppingAction::kPriceInsightsOpened:
      ukm_builder.SetPriceInsightsOpened(true);
      break;
    case ShoppingAction::kPriceTracked:
      ukm_builder.SetPriceTracked(true);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  ukm_builder.Record(ukm::UkmRecorder::Get());
}
}  // namespace commerce::metrics
