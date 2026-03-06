// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/history_sync_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_android.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/signin/public/base/signin_switches.h"

namespace {

// Impression counter for the History Sync promo ephemeral module.
const char kHistorySyncPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.history_sync_promo_counter";

// Interaction counter for the History Sync promo ephemeral module.
const char kHistorySyncPromoInteractedPref[] =
    "ephemeral_pref_interacted.history_sync_promo_interacted";

const char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

constexpr int kHistorySyncPromoId = 10;

}  // namespace

namespace segmentation_platform::home_modules {

HistorySyncPromo::HistorySyncPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kHistorySyncPromo), profile_prefs_(profile_prefs) {}

// static
void HistorySyncPromo::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kHistorySyncPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kHistorySyncPromoInteractedPref, false);
}

std::map<SignalKey, FeatureQuery> HistorySyncPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kIsEligibleToHistoryOptIn,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsEligibleToHistoryOptIn})}};

  // Define signal for number of times all educational tip card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfEducationalTipCardShownTimes,
                                kEducationalTipModuleHistogramName,
                                /* enum_id= */ nullptr, /* enum_size= */ 0,
                                /* days= */ KDaysToShowEphemeralCardOnce);
  map.emplace(kEducationalTipShownCount,
              std::move(countOfEducationalTipCardShownTimes));

  // Define signal for number of times history sync promo card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfHistorySyncPromoShownTimes,
                                kEducationalTipModuleHistogramName,
                                &kHistorySyncPromoId, /* enum_size= */ 1,
                                /* days= */ KDaysToShowEachEphemeralCardOnce);
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

  std::optional<float> result_for_history_sync_promo_shown_count =
      signals.GetSignal(kHistorySyncPromoShownCount);
  std::optional<float> result_for_is_eligible_to_history_opt_in =
      signals.GetSignal(kIsEligibleToHistoryOptIn);
  std::optional<float>
      result_for_educational_tip_shown_count_for_history_sync_signal =
          signals.GetSignal(kEducationalTipShownCount);

  if (!result_for_history_sync_promo_shown_count.has_value() ||
      !result_for_is_eligible_to_history_opt_in.has_value() ||
      !result_for_educational_tip_shown_count_for_history_sync_signal
           .has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (result_for_is_eligible_to_history_opt_in.value() &&
      result_for_history_sync_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count_for_history_sync_signal.value() <
          1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

// static
bool HistorySyncPromo::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kHistorySyncPromo == forced_result.value().result_label.value()) {
    return true;
  }

  int impression_count =
      profile_prefs->GetInteger(kHistorySyncPromoImpressionCounterPref);

  if (impression_count >= kSingleEphemeralCardMaxImpressions) {
    return false;
  }

  return true;
}

void HistorySyncPromo::OnShow(PrefService* profile_prefs,
                              PrefService* local_state) {
  // Only record an impression once per session.
  if (has_been_shown_this_session_) {
    return;
  }

  has_been_shown_this_session_ = true;

  int freshness_impression_count =
      profile_prefs->GetInteger(kHistorySyncPromoImpressionCounterPref);

  profile_prefs->SetInteger(kHistorySyncPromoImpressionCounterPref,
                            freshness_impression_count + 1);
}

void HistorySyncPromo::OnInteract(PrefService* profile_prefs,
                                  PrefService* local_state) {
  profile_prefs->SetBoolean(kHistorySyncPromoInteractedPref, true);
}

}  // namespace segmentation_platform::home_modules
