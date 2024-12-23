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

// The number of times the tab group sync promo card can be shown to the user in
// a single day.
const int kShownCountLimit = 3;

const char kTabGroupSyncPromoHistogramName[] =
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

  DEFINE_UMA_FEATURE_ENUM_COUNT(count, kTabGroupSyncPromoHistogramName,
                                &kTabGroupSyncPromoId, /* enum_size= */ 1,
                                /* days= */ 1);
  map.emplace(kTabGroupSyncPromoShownCount, std::move(count));

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

  std::optional<float> resultForSyncedTabGroupExists =
      signals.GetSignal(kSyncedTabGroupExists);
  std::optional<float> resultForTabGroupSyncPromoShownCount =
      signals.GetSignal(kTabGroupSyncPromoShownCount);

  if (!resultForSyncedTabGroupExists.has_value() ||
      !resultForTabGroupSyncPromoShownCount.has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (*resultForSyncedTabGroupExists &&
      resultForTabGroupSyncPromoShownCount.value() < kShownCountLimit) {
    result.position = EphemeralHomeModuleRank::kTop;
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

  if (impression_count >= features::kMaxTabGroupSyncCardImpressions.Get()) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
