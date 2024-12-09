// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tab_group_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

// The number of tabs required to make the tab group promo card visible to
// users.
const int kTabCountLimit = 10;

// The number of times the tab group promo card can be shown to the user in a
// single day.
const int kShownCountLimit = 3;

const char kTabGroupPromoHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.EducationalTip.Impression";

// TODO(crbug.com/382803396): The enum id of the tab group promo card. Could be
// referenced after refactor.
const int kTabGroupPromoId = 7;

}  // namespace

namespace segmentation_platform::home_modules {

TabGroupPromo::TabGroupPromo(PrefService* profile_prefs)
    : CardSelectionInfo(kTabGroupPromo), profile_prefs_(profile_prefs) {}

std::map<SignalKey, FeatureQuery> TabGroupPromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kNumberOfTabs,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kNumberOfTabs})},
      {kTabGroupExists,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kTabGroupExists})},
  };

  DEFINE_UMA_FEATURE_ENUM_COUNT(count, kTabGroupPromoHistogramName,
                                &kTabGroupPromoId, /* enum_size= */ 1,
                                /* days= */ 1);
  map.emplace(kTabGroupPromoShownCount, std::move(count));

  return map;
}

CardSelectionInfo::ShowResult TabGroupPromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  CardSelectionInfo::ShowResult result;
  result.result_label = kTabGroupPromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kTabGroupPromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> resultForTabGroupExists =
      signals.GetSignal(kTabGroupExists);
  std::optional<float> resultForNumberOfTabs = signals.GetSignal(kNumberOfTabs);
  std::optional<float> resultForTabGroupPromoShownCount =
      signals.GetSignal(kTabGroupPromoShownCount);

  if (!resultForTabGroupExists.has_value() ||
      !resultForNumberOfTabs.has_value() ||
      !resultForTabGroupPromoShownCount.has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  if (!*resultForTabGroupExists &&
      resultForNumberOfTabs.value() > kTabCountLimit &&
      resultForTabGroupPromoShownCount.value() < kShownCountLimit) {
    result.position = EphemeralHomeModuleRank::kTop;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool TabGroupPromo::IsEnabled(int impression_count) {
  if (!base::FeatureList::IsEnabled(features::kEducationalTipModule)) {
    return false;
  }

  if (impression_count >= features::kMaxTabGroupCardImpressions.Get()) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
