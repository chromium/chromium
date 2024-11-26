// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/address_bar_position_ephemeral_module.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
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

// Defines the signals that, if any are present and evaluate to true, will
// prevent `AddressBarPositionEphemeralModule` from being shown.
constexpr auto kDisqualifyingSignals =
    base::MakeFixedFlatSet<std::string_view>({
        segmentation_platform::kIsNewUser,
    });

}  // namespace

// static
bool AddressBarPositionEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kAddressBarPositionEphemeralModule;
}

// static
bool AddressBarPositionEphemeralModule::IsEnabled(int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  // If forced to show/hide and the module label matches the current module,
  // return true/false accordingly.
  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      AddressBarPositionEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value().position == EphemeralHomeModuleRank::kTop;
  }

  int max_impression_count =
      features::GetTipsEphemeralCardModuleMaxImpressionCount();

  return impression_count < max_impression_count;
}

// Defines the input signals required by this module.
std::map<SignalKey, FeatureQuery>
AddressBarPositionEphemeralModule::GetInputs() {
  return {
      {segmentation_platform::kDidNotSeeAddressBarPositionChoiceScreen,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kDidNotSeeAddressBarPositionChoiceScreen)},
      {segmentation_platform::kIsNewUser,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kIsNewUser)},
      {segmentation_platform::kIsPhoneFormFactor,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kIsPhoneFormFactor)},
  };
}

CardSelectionInfo::ShowResult
AddressBarPositionEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      AddressBarPositionEphemeralModule::IsModuleLabel(
          forced_result.value().result_label.value())) {
    return forced_result.value();
  }

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

  // Checks if any of the disqualifying signals are present and have a positive
  // value in the provided `signals`.
  for (const auto& signal : kDisqualifyingSignals) {
    std::optional<float> result = signals.GetSignal(std::string(signal));

    if (result.has_value() && result.value() > 0) {
      return ShowResult(EphemeralHomeModuleRank::kNotShown);
    }
  }

  return ShowResult(EphemeralHomeModuleRank::kTop,
                    kAddressBarPositionEphemeralModule);
}

}  // namespace segmentation_platform::home_modules
