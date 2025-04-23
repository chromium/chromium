// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/history_sync_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/signin/public/base/signin_switches.h"
namespace {

// The number of times the history sync promo card can be shown to the user in
// a single day.
constexpr int kDailyShownCountLimit = 3;

constexpr char kHistorySyncPromoHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

constexpr int kHistorySyncPromoId = 10;

}  // namespace

namespace segmentation_platform::home_modules {

HistorySyncPromo::HistorySyncPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kHistorySyncPromo), profile_prefs_(profile_prefs) {}

std::map<SignalKey, FeatureQuery> HistorySyncPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kIsEligibleToHistoryOptIn,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsEligibleToHistoryOptIn})}};

  // Define signal for number of times history sync promo card has shown to the
  // user in last 24 hours.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfHistorySyncPromoShownTimes,
                                kHistorySyncPromoHistogramName,
                                &kHistorySyncPromoId,
                                /* enum_size= */ 1,
                                /* days= */ 1);
  map.emplace(kHistorySyncPromoShownCount,
              std::move(countOfHistorySyncPromoShownTimes));
  return map;
}

//  When not forced to be shown by flags and has less than 10 impressions, the
//  history sync promo will be shown to users if
//  1. The user has not already interacted with the card.
//  2. The number of impressions in 24 hours doesn't exceed the cap
//  (kDailyShownCountLimit)
//  3. kIsEligibleToHistoryOptIn returns true
CardSelectionInfo::ShowResult HistorySyncPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kHistorySyncPromo == forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kHistorySyncPromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kHistorySyncPromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> resultForHistorySyncPromoShownCount =
      signals.GetSignal(kHistorySyncPromoShownCount);
  std::optional<float> resultForIsEligibleToHistoryOptIn =
      signals.GetSignal(kIsEligibleToHistoryOptIn);

  if (!resultForHistorySyncPromoShownCount.has_value() ||
      !resultForIsEligibleToHistoryOptIn.has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (resultForHistorySyncPromoShownCount.value() < kDailyShownCountLimit &&
      resultForIsEligibleToHistoryOptIn.value()) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool HistorySyncPromo::IsEnabled(int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kHistorySyncPromo == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kEducationalTipModule) ||
      !base::FeatureList::IsEnabled(switches::kHistoryOptInEducationalTip)) {
    return false;
  }

  if (impression_count >= features::kMaxHistorySyncCardImpressions.Get()) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
