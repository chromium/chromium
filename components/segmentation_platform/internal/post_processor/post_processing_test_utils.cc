// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"

#include "components/segmentation_platform/internal/metadata/metadata_writer.h"

namespace segmentation_platform::test_utils {

// Labels for BinaryClassifier.
const char kNotShowShare[] = "Not Show Share";
const char kShowShare[] = "Show Share";

// TTL for BinaryClassifier labels.
const int kShowShareTTL = 3;
const int kDefaultTTL = 5;

// Labels for BinnedClassifier.
const char kLowUsed[] = "Low";
const char kMediumUsed[] = "Medium";
const char kHighUsed[] = "High";
const char kUnderflowLabel[] = "Underflow";

// Labels for MultiClassClassifier.
const char kNewTabUser[] = "NewTab";
const char kShareUser[] = "Share";
const char kShoppingUser[] = "Shopping";
const char kVoiceUser[] = "Voice";

std::unique_ptr<Config> CreateTestConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = "client_key";
  config->segmentation_uma_name = "test_key";
  config->segment_selection_ttl = base::Days(28);
  config->unknown_selection_ttl = base::Days(14);
  config->auto_execute_and_cache = true;
  config->AddSegmentId(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  return config;
}

std::unique_ptr<Config> CreateTestConfig(const std::string& client_key,
                                         proto::SegmentId segment_id) {
  auto config = std::make_unique<Config>();
  config->segmentation_key = client_key;
  config->segmentation_uma_name = client_key;
  config->auto_execute_and_cache = true;
  config->segment_selection_ttl = base::Days(28);
  config->unknown_selection_ttl = base::Days(14);
  config->AddSegmentId(segment_id);
  return config;
}

proto::OutputConfig GetTestOutputConfigForBinaryClassifier(
    bool ignore_previous_model_ttl) {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);

  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5, /*positive_label=*/kShowShare,
      /*negative_label=*/kNotShowShare);

  if (ignore_previous_model_ttl) {
    writer.SetIgnorePreviousModelTTLInOutputConfig();
  }

  writer.AddPredictedResultTTLInOutputConfig({{kShowShare, kShowShareTTL}},
                                             kDefaultTTL, proto::TimeUnit::DAY);

  return model_metadata.output_config();
}

proto::OutputConfig GetTestOutputConfigForBinnedClassifier() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{0.2, kLowUsed}, {0.3, kMediumUsed}, {0.5, kHighUsed}},
      kUnderflowLabel);
  return model_metadata.output_config();
}

proto::OutputConfig GetTestOutputConfigForMultiClassClassifier(
    int top_k_outputs,
    std::optional<float> threshold) {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);

  std::array<const char*, 4> labels{kShareUser, kNewTabUser, kVoiceUser,
                                    kShoppingUser};
  writer.AddOutputConfigForMultiClassClassifier(labels, top_k_outputs,
                                                threshold);
  return model_metadata.output_config();
}

proto::OutputConfig GetTestOutputConfigForGenericPredictor(
    const std::vector<std::string>& labels) {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForGenericPredictor(labels);
  return model_metadata.output_config();
}

proto::ClientResult CreateClientResult(proto::PredictionResult pred_result) {
  proto::ClientResult client_result;
  client_result.mutable_client_result()->CopyFrom(pred_result);
  client_result.set_timestamp_us(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  return client_result;
}

}  // namespace segmentation_platform::test_utils
