// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/compose_promotion.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/compose/core/browser/config.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for ComposePromotion model.
constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION;
constexpr int64_t kModelVersion = 1;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// No signals are being collected, so no collection length is required.
constexpr int64_t kMinSignalCollectionLength = 0;

}  // namespace

// static
std::unique_ptr<Config> ComposePromotion::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformComposePromotion)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kComposePromotionKey;
  config->segmentation_uma_name = kComposePromotionUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<ComposePromotion>());
  config->auto_execute_and_cache = false;
  return config;
}

ComposePromotion::ComposePromotion() : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
ComposePromotion::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_RANDOM,
      .name = "random"});

  writer.AddOutputConfigForBinaryClassifier(
      0.5,
      /*positive_label=*/kComposePrmotionLabelShow,
      kComposePrmotionLabelDontShow);
  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void ComposePromotion::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 1) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  float result = 0;
  if (inputs[0] <
      compose::GetComposeConfig().proactive_nudge_show_probability) {
    result = 1;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
