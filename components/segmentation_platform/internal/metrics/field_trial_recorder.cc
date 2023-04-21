// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/metrics/field_trial_recorder.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"

namespace segmentation_platform {

FieldTrialRecorder::FieldTrialRecorder(FieldTrialRegister* field_trial_register)
    : field_trial_register_(field_trial_register) {}

FieldTrialRecorder::~FieldTrialRecorder() = default;

void FieldTrialRecorder::RecordFieldTrialAtStartup(
    const std::vector<std::unique_ptr<Config>>& configs,
    CachedResultProvider* cached_result_provider) {
  for (const auto& config : configs) {
    if (config->on_demand_execution ||
        metadata_utils::ConfigUsesLegacyOutput(config.get())) {
      continue;
    }

    const std::string& trial_name = config->GetSegmentationFilterName();
    ClassificationResult cached_results =
        cached_result_provider->GetCachedResultForClient(
            config->segmentation_key);
    const std::string& group_name = cached_results.ordered_labels.size() > 0
                                        ? cached_results.ordered_labels[0]
                                        : "Unselected";
    field_trial_register_->RegisterFieldTrial(trial_name, group_name);
  }
}

}  // namespace segmentation_platform
