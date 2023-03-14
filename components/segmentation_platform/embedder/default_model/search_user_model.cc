// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/search_user_model.h"

#include <array>

#include "base/feature_list.h"
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

  // Set features.
  writer.AddUmaFeatures(kSearchUserUMAFeatures.data(),
                        kSearchUserUMAFeatures.size());

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
