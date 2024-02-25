// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/metrics/field_trial_recorder.h"

#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/post_processor/post_processor.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"

namespace segmentation_platform {

FieldTrialRecorder::FieldTrialRecorder(FieldTrialRegister* field_trial_register)
    : field_trial_register_(field_trial_register) {}

FieldTrialRecorder::~FieldTrialRecorder() = default;

void FieldTrialRecorder::RecordFieldTrialAtStartup(
    const std::vector<std::unique_ptr<Config>>& configs,
    CachedResultProvider* cached_result_provider) {
  for (const auto& config : configs) {
    if (metadata_utils::ConfigUsesLegacyOutput(config.get())) {
      continue;
    }

    std::string trial_name = config->GetSegmentationFilterName();
    std::optional<proto::PredictionResult> cached_results =
        cached_result_provider->GetPredictionResultForClient(
            config->segmentation_key);
    std::string group_name = "Unselected";
    if (cached_results &&
        PostProcessor::IsClassificationResult(*cached_results)) {
      std::vector<std::string> ordered_labels =
          PostProcessor().GetClassifierResults(*cached_results);
      if (ordered_labels.size() > 0) {
        group_name = ordered_labels[0];
      }
    }
    field_trial_register_->RegisterFieldTrial(trial_name, group_name);
  }
}

}  // namespace segmentation_platform
