// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/resume_heavy_user_model.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for the model.
constexpr SegmentId kSegmentId = SegmentId::RESUME_HEAVY_USER_SEGMENT;
constexpr int64_t kModelVersion = 2;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 5> kUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileBookmarkManagerOpen", 7),
    MetadataWriter::UMAFeature::FromUserAction("NewTabPage.MostVisited.Clicked",
                                               7),
    MetadataWriter::UMAFeature::FromUserAction("TabGroup.Created.OpenInNewTab",
                                               7),
    MetadataWriter::UMAFeature::FromUserAction("Android.HistoryPage.OpenItem",
                                               7),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuRecentTabs", 7),
};

}  // namespace

// static
std::unique_ptr<Config> ResumeHeavyUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(features::kResumeHeavyUserSegmentFeature)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kResumeHeavyUserKey;
  config->segmentation_uma_name = kResumeHeavyUserUmaName;
  config->AddSegmentId(SegmentId::RESUME_HEAVY_USER_SEGMENT,
                       std::make_unique<ResumeHeavyUserModel>());
  config->auto_execute_and_cache = true;
  config->is_boolean_segment = true;

  return config;
}

ResumeHeavyUserModel::ResumeHeavyUserModel()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
ResumeHeavyUserModel::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      /*min_signal_collection_length_days=*/7,
      /*signal_storage_length_days=*/14);

  // Set OutputConfig.
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5f,
      /*positive_label=*/SegmentIdToHistogramVariant(kSegmentId),
      /*negative_label=*/kLegacyNegativeLabel);

  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/14,
      /*time_unit=*/proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());
  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void ResumeHeavyUserModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  const int bookmarks_opened = inputs[0];
  const int mv_tiles_clicked = inputs[1];
  const int opened_ntp_from_tab_groups = inputs[2];
  const int opened_item_from_history = inputs[3];
  const int opened_recent_tabs = inputs[4];
  float result = 0;

  // Determine if the user has used chrome features to resume workflow.
  if (bookmarks_opened >= 2 || mv_tiles_clicked >= 2 ||
      opened_ntp_from_tab_groups >= 2 || opened_item_from_history >= 2 ||
      opened_recent_tabs >= 2) {
    result = 1;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
