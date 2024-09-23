// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform {

namespace {

using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kChromeStartSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
constexpr int64_t kChromeStartSignalStorageLength = 28;
constexpr int64_t kChromeStartMinSignalCollectionLength = 28;
constexpr int64_t kModelVersion = 2;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kChromeStartUMAFeatures = {
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_VALUE,
        .name = "Session.TotalDuration",
        .bucket_count = 28,
        .tensor_length = 28,
        .aggregation = proto::Aggregation::BUCKETED_COUNT,
        .enum_ids_size = 0}};

}  // namespace

LowUserEngagementModel::LowUserEngagementModel()
    : DefaultModelProvider(kChromeStartSegmentId) {}

std::unique_ptr<Config> LowUserEngagementModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformLowEngagementFeature)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeLowUserEngagementSegmentationKey;
  config->segmentation_uma_name = kChromeLowUserEngagementUmaName;
  config->AddSegmentId(kChromeStartSegmentId,
                       std::make_unique<LowUserEngagementModel>());
  config->auto_execute_and_cache = true;
  config->is_boolean_segment = true;

  return config;
}

std::unique_ptr<DefaultModelProvider::ModelConfig>
LowUserEngagementModel::GetModelConfig() {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kChromeStartMinSignalCollectionLength, kChromeStartSignalStorageLength);

  // Set features.
  writer.AddUmaFeatures(kChromeStartUMAFeatures.data(),
                        kChromeStartUMAFeatures.size());

  // Set OutputConfig.
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5f,
      /*positive_label=*/kChromeLowUserEngagementUmaName,
      /*negative_label=*/kLegacyNegativeLabel);

  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/7,
      /*time_unit=*/proto::TimeUnit::DAY);

  return std::make_unique<ModelConfig>(std::move(chrome_start_metadata),
                                       kModelVersion);
}

void LowUserEngagementModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 28) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  float result = 0;
  bool weeks[4]{};
  for (unsigned i = 0; i < 28; ++i) {
    int week_idx = i / 7;
    weeks[week_idx] = weeks[week_idx] || inputs[i];
  }
  if (!weeks[0] || !weeks[1] || !weeks[2] || !weeks[3])
    result = 1;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
