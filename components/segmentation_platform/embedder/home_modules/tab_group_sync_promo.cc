// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tab_group_sync_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// Impression counter for the Tab Group Sync promo ephemeral module.
const char kTabGroupSyncPromoImpressionCounterPref[] =
    "ephemeral_pref_counter.tab_group_sync_promo_counter";

// Interaction counter for the Tab Group Sync promo ephemeral module.
const char kTabGroupSyncPromoInteractedPref[] =
    "ephemeral_pref_interacted.tab_group_sync_promo_interacted";

const char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

// TODO(crbug.com/382803396): The enum id of the tab group sync promo card.
// Could be referenced after refactor.
const int kTabGroupSyncPromoId = 8;

}  // namespace

namespace segmentation_platform::home_modules {

TabGroupSyncPromo::TabGroupSyncPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kTabGroupSyncPromo), profile_prefs_(profile_prefs) {}

// static
void TabGroupSyncPromo::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTabGroupSyncPromoImpressionCounterPref, 0);
  registry->RegisterBooleanPref(kTabGroupSyncPromoInteractedPref, false);
}

std::map<SignalKey, FeatureQuery> TabGroupSyncPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kSyncedTabGroupExists,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kSyncedTabGroupExists})},
  };

  // Define signal for number of times all educational tip card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfEducationalTipCardShownTimes,
                                kEducationalTipModuleHistogramName,
                                /* enum_id= */ nullptr, /* enum_size= */ 0,
                                /* days= */ KDaysToShowEphemeralCardOnce);
  map.emplace(kEducationalTipShownCount,
              std::move(countOfEducationalTipCardShownTimes));

  // Define signal for number of times tab group sync promo card has shown to
  // the user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfTabGroupSyncPromoShownTimes,
                                kEducationalTipModuleHistogramName,
                                &kTabGroupSyncPromoId, /* enum_size= */ 1,
                                /* days= */ KDaysToShowEachEphemeralCardOnce);
  map.emplace(kTabGroupSyncPromoShownCount,
              std::move(countOfTabGroupSyncPromoShownTimes));

  return map;
}

CardSelectionInfo::ShowResult TabGroupSyncPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kTabGroupSyncPromo == forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kTabGroupSyncPromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kTabGroupSyncPromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> result_for_synced_tab_group_exists =
      signals.GetSignal(kSyncedTabGroupExists);
  std::optional<float> result_for_tab_group_sync_promo_shown_count =
      signals.GetSignal(kTabGroupSyncPromoShownCount);
  std::optional<float>
      result_for_educational_tip_shown_count_for_tab_group_sync_signal =
          signals.GetSignal(kEducationalTipShownCount);

  if (!result_for_synced_tab_group_exists.has_value() ||
      !result_for_tab_group_sync_promo_shown_count.has_value() ||
      !result_for_educational_tip_shown_count_for_tab_group_sync_signal
           .has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (*result_for_synced_tab_group_exists &&
      result_for_tab_group_sync_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count_for_tab_group_sync_signal.value() <
          1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

// static
bool TabGroupSyncPromo::IsEnabled(PrefService* profile_prefs) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kTabGroupSyncPromo == forced_result.value().result_label.value()) {
    return true;
  }

  int impression_count =
      profile_prefs->GetInteger(kTabGroupSyncPromoImpressionCounterPref);

  if (impression_count >= kSingleEphemeralCardMaxImpressions) {
    return false;
  }

  return true;
}

void TabGroupSyncPromo::OnShow(PrefService* profile_prefs,
                               PrefService* local_state) {
  // Only record an impression once per session.
  if (has_been_shown_this_session_) {
    return;
  }

  has_been_shown_this_session_ = true;

  int freshness_impression_count =
      profile_prefs->GetInteger(kTabGroupSyncPromoImpressionCounterPref);

  profile_prefs->SetInteger(kTabGroupSyncPromoImpressionCounterPref,
                            freshness_impression_count + 1);
}

void TabGroupSyncPromo::OnInteract(PrefService* profile_prefs,
                                   PrefService* local_state) {
  profile_prefs->SetBoolean(kTabGroupSyncPromoInteractedPref, true);
}

}  // namespace segmentation_platform::home_modules
