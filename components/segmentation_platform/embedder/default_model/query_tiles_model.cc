// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/query_tiles_model.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/query_tiles/switches.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for query tiles model.
constexpr SegmentId kQueryTilesSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES;
constexpr int64_t kQueryTilesSignalStorageLength = 28;
constexpr int64_t kQueryTilesMinSignalCollectionLength = 7;
constexpr int64_t kMvThreshold = 1;

// See
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/query_tiles/QueryTileUtils.java
const char kNumDaysKeepShowingQueryTiles[] =
    "num_days_keep_showing_query_tiles";
const char kNumDaysMVCkicksBelowThreshold[] =
    "num_days_mv_clicks_below_threshold";

// DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES
constexpr int kQueryTilesDefaultSelectionTTLDays = 28;
// DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD
constexpr int kQueryTilesDefaultUnknownTTLDays = 7;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 2> kQueryTilesUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 7),
    MetadataWriter::UMAFeature::FromUserAction(
        "Search.QueryTiles.NTP.Tile.Clicked",
        7)};

std::unique_ptr<DefaultModelProvider> GetQueryTilesDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          query_tiles::features::kQueryTilesSegmentation,
          kDefaultModelEnabledParam, true)) {
    return nullptr;
  }
  return std::make_unique<QueryTilesModel>();
}

}  // namespace

// static
std::unique_ptr<Config> QueryTilesModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          query_tiles::features::kQueryTilesSegmentation)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kQueryTilesSegmentationKey;
  config->segmentation_uma_name = kQueryTilesUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES,
                       GetQueryTilesDefaultModel());
  config->auto_execute_and_cache = true;

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysKeepShowingQueryTiles, kQueryTilesDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysMVCkicksBelowThreshold, kQueryTilesDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);
  config->is_boolean_segment = true;
  return config;
}

QueryTilesModel::QueryTilesModel()
    : DefaultModelProvider(kQueryTilesSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
QueryTilesModel::GetModelConfig() {
  proto::SegmentationModelMetadata query_tiles_metadata;
  MetadataWriter writer(&query_tiles_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kQueryTilesMinSignalCollectionLength, kQueryTilesSignalStorageLength);

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMapping(kQueryTilesSegmentationKey);

  // Set features.
  writer.AddUmaFeatures(kQueryTilesUMAFeatures.data(),
                        kQueryTilesUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(query_tiles_metadata),
                                       /*model_version=*/2);
}

void QueryTilesModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kQueryTilesUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
