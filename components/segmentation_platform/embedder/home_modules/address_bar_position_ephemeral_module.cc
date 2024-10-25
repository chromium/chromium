// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/address_bar_position_ephemeral_module.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::home_modules {

namespace {

// Defines the signals that must all evaluate to true for
// `AddressBarPositionEphemeralModule` to be shown.
constexpr auto kRequiredSignals = base::MakeFixedFlatSet<std::string_view>({
    segmentation_platform::kDidNotSeeAddressBarPositionChoiceScreen,
    segmentation_platform::kIsPhoneFormFactor,
});

}  // namespace

// static
bool AddressBarPositionEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kAddressBarPositionEphemeralModule;
}

// static
bool AddressBarPositionEphemeralModule::IsEnabled(int impression_count) {
  // Check if an AddressBarPosition tip variation is being forced via a Finch
  // feature param.
  std::string force_show_param = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceShowCardParam, "");

  if (AddressBarPositionEphemeralModule::IsModuleLabel(force_show_param)) {
    // Force enabled if the param matches this module.
    return true;
  }

  std::string force_hide_param = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceHideCardParam, "");

  if (AddressBarPositionEphemeralModule::IsModuleLabel(force_hide_param)) {
    // Force disabled if the param matches this module.
    return false;
  }

  int max_impression_count =
      features::GetTipsEphemeralCardModuleMaxImpressionCount();

  return impression_count <= max_impression_count;
}

// Defines the input signals required by this module.
std::map<SignalKey, FeatureQuery>
AddressBarPositionEphemeralModule::GetInputs() {
  return {
      {segmentation_platform::kDidNotSeeAddressBarPositionChoiceScreen,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kDidNotSeeAddressBarPositionChoiceScreen)},
      {segmentation_platform::kIsPhoneFormFactor,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kIsPhoneFormFactor)},
  };
}

CardSelectionInfo::ShowResult
AddressBarPositionEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  bool has_been_interacted_with = profile_prefs_->GetBoolean(
      kAddressBarPositionEphemeralModuleInteractedPref);

  if (has_been_interacted_with) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown);
  }

  // Checks if all the required signals are present and have a positive value in
  // the provided `signals`.
  for (const auto& signal : kRequiredSignals) {
    std::optional<float> result = signals.GetSignal(std::string(signal));

    if (!result.has_value() || result.value() <= 0) {
      return ShowResult(EphemeralHomeModuleRank::kNotShown);
    }
  }

  return ShowResult(EphemeralHomeModuleRank::kTop,
                    kAddressBarPositionEphemeralModule);
}

}  // namespace segmentation_platform::home_modules
