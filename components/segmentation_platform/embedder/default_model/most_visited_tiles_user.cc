// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/most_visited_tiles_user.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

BASE_FEATURE(kSegmentationPlatformMostVisitedTilesUser,
             "SegmentationPlatformMostVisitedTilesUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {
using proto::SegmentId;

enum class MvtUserBin {
  kHigh = 4,
  kMedium = 3,
  kLow = 2,
  kNone = 1,
  kUnknown = 0
};

#define RANK(x) static_cast<int>(x)

// Default parameters for MostVisitedTilesUser model.
constexpr SegmentId kSegmentId = SegmentId::MOST_VISITED_TILES_USER;
constexpr int64_t kModelVersion = 1;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 7 days of data.
constexpr int64_t kMinSignalCollectionLength = 7;
// Refresh the result every 7 days.
constexpr int64_t kResultTTLDays = 7;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 28),
};

}  // namespace

// static
std::unique_ptr<Config> MostVisitedTilesUser::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          kSegmentationPlatformMostVisitedTilesUser)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kMostVisitedTilesUserKey;
  config->segmentation_uma_name = kMostVisitedTilesUserUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<MostVisitedTilesUser>());
  config->auto_execute_and_cache = true;
  return config;
}

MostVisitedTilesUser::MostVisitedTilesUser()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
MostVisitedTilesUser::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  // Set output config.
  writer.AddOutputConfigForBinnedClassifier(
      {{RANK(MvtUserBin::kNone), "None"},
       {RANK(MvtUserBin::kLow), "Low"},
       {RANK(MvtUserBin::kMedium), "Medium"},
       {RANK(MvtUserBin::kHigh), "High"}},
      "None");
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kResultTTLDays, proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void MostVisitedTilesUser::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  float result = 0;
  const int mvt_usage = inputs[0];

  if (mvt_usage >= 8) {
    result = RANK(MvtUserBin::kHigh);
  } else if (mvt_usage >= 4) {
    result = RANK(MvtUserBin::kMedium);
  } else if (mvt_usage >= 1) {
    result = RANK(MvtUserBin::kLow);
  } else {
    result = RANK(MvtUserBin::kNone);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
