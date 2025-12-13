// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tab_group_sync_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

const char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

// TODO(crbug.com/382803396): The enum id of the tab group sync promo card.
// Could be referenced after refactor.
const int kTabGroupSyncPromoId = 8;

}  // namespace

namespace segmentation_platform::home_modules {

TabGroupSyncPromo::TabGroupSyncPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kTabGroupSyncPromo), profile_prefs_(profile_prefs) {}

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

bool TabGroupSyncPromo::IsEnabled(int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kTabGroupSyncPromo == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kEducationalTipModule)) {
    return false;
  }

  if (impression_count >= kSingleEphemeralCardMaxImpressions) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
