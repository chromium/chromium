// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform::home_modules {

std::optional<CardSelectionInfo::ShowResult>
GetForcedEphemeralModuleShowResult() {
  std::string force_show_param = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceShowCardParam, "");

  if (!force_show_param.empty()) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kTop,
                                         force_show_param);
  }

  std::string force_hide_param = base::GetFieldTrialParamByFeatureAsString(
      features::kSegmentationPlatformEphemeralCardRanker,
      features::kEphemeralCardRankerForceHideCardParam, "");

  if (!force_hide_param.empty()) {
    return CardSelectionInfo::ShowResult(EphemeralHomeModuleRank::kNotShown,
                                         force_hide_param);
  }

  return std::nullopt;
}

}  // namespace segmentation_platform::home_modules
