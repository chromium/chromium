// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/save_passwords_ephemeral_module.h"

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
// `SavePasswordsEphemeralModule` to be shown.
constexpr auto kRequiredSignals = base::MakeFixedFlatSet<std::string_view>({
    segmentation_platform::kNoSavedPasswords,
    segmentation_platform::kPasswordManagerAllowedByEnterprisePolicy,
});

}  // namespace

// static
bool SavePasswordsEphemeralModule::IsModuleLabel(std::string_view label) {
  return label == kSavePasswordsEphemeralModule;
}

// static
bool SavePasswordsEphemeralModule::IsEnabled(int impression_count) {
  // Check if a SavePasswords tip variation is being forced via a Finch
  // feature param.
  std::string force_show_param = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceShowCardParam, "");

  if (SavePasswordsEphemeralModule::IsModuleLabel(force_show_param)) {
    // Force enabled if the param matches this module.
    return true;
  }

  std::string force_hide_param = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceHideCardParam, "");

  if (SavePasswordsEphemeralModule::IsModuleLabel(force_hide_param)) {
    // Force disabled if the param matches this module.
    return false;
  }

  int max_impression_count =
      features::GetTipsEphemeralCardModuleMaxImpressionCount();

  return impression_count <= max_impression_count;
}

// Defines the input signals required by this module.
std::map<SignalKey, FeatureQuery> SavePasswordsEphemeralModule::GetInputs() {
  return {
      {segmentation_platform::kNoSavedPasswords,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kNoSavedPasswords)},
      {segmentation_platform::kPasswordManagerAllowedByEnterprisePolicy,
       CreateFeatureQueryFromCustomInputName(
           segmentation_platform::kPasswordManagerAllowedByEnterprisePolicy)},
  };
}

CardSelectionInfo::ShowResult SavePasswordsEphemeralModule::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kSavePasswordsEphemeralModuleInteractedPref);

  if (has_been_interacted_with) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown);
  }

  // Checks if all the required signals are present and have a positive value
  // in the provided `signals`.
  for (const auto& signal : kRequiredSignals) {
    std::optional<float> result = signals.GetSignal(std::string(signal));

    if (!result.has_value() || result.value() <= 0) {
      return ShowResult(EphemeralHomeModuleRank::kNotShown);
    }
  }

  return ShowResult(EphemeralHomeModuleRank::kTop,
                    kSavePasswordsEphemeralModule);
}

}  // namespace segmentation_platform::home_modules
