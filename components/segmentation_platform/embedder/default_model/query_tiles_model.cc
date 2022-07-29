// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/query_tiles_model.h"

#include <array>

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for query tiles model.
constexpr SegmentId kQueryTilesSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES;
constexpr proto::TimeUnit kQueryTilesTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kQueryTilesBucketDuration = 1;
constexpr int64_t kQueryTilesSignalStorageLength = 28;
constexpr int64_t kQueryTilesMinSignalCollectionLength = 7;
constexpr int64_t kQueryTilesResultTTL = 1;
constexpr int64_t kMvThreshold = 1;

// Discrete mapping parameters.
constexpr char kQueryTilesDiscreteMappingKey[] = "query_tiles";
constexpr float kQueryTilesDiscreteMappingMinResult = 1;
constexpr int64_t kQueryTilesDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kQueryTilesDiscreteMappingMinResult, kQueryTilesDiscreteMappingRank}};

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 2> kQueryTilesUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 7),
    MetadataWriter::UMAFeature::FromUserAction(
        "Search.QueryTiles.NTP.Tile.Clicked",
        7)};

}  // namespace

QueryTilesModel::QueryTilesModel() : ModelProvider(kQueryTilesSegmentId) {}

void QueryTilesModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata query_tiles_metadata;
  MetadataWriter writer(&query_tiles_metadata);
  writer.SetSegmentationMetadataConfig(
      kQueryTilesTimeUnit, kQueryTilesBucketDuration,
      kQueryTilesSignalStorageLength, kQueryTilesMinSignalCollectionLength,
      kQueryTilesResultTTL);

  // Set discrete mapping.
  writer.AddDiscreteMappingEntries(kQueryTilesDiscreteMappingKey,
                                   kDiscreteMappings, 1);

  // Set features.
  writer.AddUmaFeatures(kQueryTilesUMAFeatures.data(),
                        kQueryTilesUMAFeatures.size());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kQueryTilesSegmentId,
                          std::move(query_tiles_metadata), 2));
}

void QueryTilesModel::ExecuteModelWithInput(const std::vector<float>& inputs,
                                            ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kQueryTilesUMAFeatures.size()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  const int mv_clicks = inputs[0];
  const int query_tiles_clicks = inputs[1];
  float result = 0;

  // If mv clicks are below the threshold or below the query tiles clicks, query
  // tiles should be enabled.
  if (mv_clicks <= kMvThreshold || mv_clicks <= query_tiles_clicks) {
    result = 1;  // Enable query tiles;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool QueryTilesModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
