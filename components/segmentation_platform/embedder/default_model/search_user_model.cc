// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/search_user_model.h"

#include <array>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for search user model.
constexpr uint64_t kSearchUserModelVersion = 2;
constexpr SegmentId kSearchUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;
constexpr int64_t kSearchUserSignalStorageLength = 28;
constexpr int64_t kSearchUserMinSignalCollectionLength = 7;

// Reference to the UMA ClientSummarizedResultType enum for Search.
constexpr std::array<int32_t, 1> kOnlySearch{1};

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kSearchUserUMAFeatures = {
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        28,
        kOnlySearch.data(),
        kOnlySearch.size()),
};

constexpr char kUkmInputEnabled[] = "ukm-input-enabled";

std::unique_ptr<ModelProvider> GetSearchUserDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformSearchUser, kDefaultModelEnabledParam,
          true)) {
    return nullptr;
  }
  return std::make_unique<SearchUserModel>();
}

}  // namespace

// static
std::unique_ptr<Config> SearchUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformSearchUser)) {
    return nullptr;
  }

  auto config = std::make_unique<Config>();
  config->segmentation_key = kSearchUserKey;
  config->segmentation_uma_name = kSearchUserUmaName;
  config->AddSegmentId(kSearchUserSegmentId, GetSearchUserDefaultModel());
  return config;
}

SearchUserModel::SearchUserModel() : ModelProvider(kSearchUserSegmentId) {}

void SearchUserModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata search_user_metadata;
  MetadataWriter writer(&search_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kSearchUserMinSignalCollectionLength, kSearchUserSignalStorageLength);
  search_user_metadata.set_upload_tensors(true);

  // Set features.
  writer.AddUmaFeatures(kSearchUserUMAFeatures.data(),
                        kSearchUserUMAFeatures.size());

  if (base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformSearchUser, kUkmInputEnabled, false)) {
    std::string query =
        "SELECT COUNT(id) FROM metrics WHERE metric_hash = '64BD7CCE5A95BF00'";
    const std::array<UkmMetricHash, 1> kNavigationMetric = {
        UkmMetricHash::FromUnsafeValue(7259095400115977984ull)};
    const std::array<MetadataWriter::SqlFeature::EventAndMetrics, 1>
        kPageLoadEvent{MetadataWriter::SqlFeature::EventAndMetrics{
            .event_hash =
                UkmEventHash::FromUnsafeValue(12426032810838168341ull),
            .metrics = kNavigationMetric.data(),
            .metrics_size = kNavigationMetric.size()}};

    MetadataWriter::SqlFeature sql_feature{
        .sql = query.c_str(),
        .events = kPageLoadEvent.data(),
        .events_size = kPageLoadEvent.size()};
    MetadataWriter::SqlFeature features[] = {sql_feature};
    writer.AddSqlFeatures(features, 1);
  }

  // Set OutputConfig.
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{1, kSearchUserModelLabelLow},
                {5, kSearchUserModelLabelMedium},
                {22, kSearchUserModelLabelHigh}},
      /*underflow_label=*/kSearchUserModelLabelNone);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/7,
      /*time_unit=*/proto::TimeUnit::DAY);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindRepeating(
                     model_updated_callback, kSearchUserSegmentId,
                     std::move(search_user_metadata), kSearchUserModelVersion));
}

void SearchUserModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kSearchUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }
  auto search_count = inputs[0];

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                ModelProvider::Response(1, search_count)));
}

bool SearchUserModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
