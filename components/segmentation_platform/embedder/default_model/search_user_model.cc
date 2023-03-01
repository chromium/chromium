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
constexpr uint64_t kSearchUserModelVersion = 1;
constexpr SegmentId kSearchUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;
constexpr int64_t kSearchUserSignalStorageLength = 28;
constexpr int64_t kSearchUserMinSignalCollectionLength = 7;
constexpr int kSearchUserSegmentSelectionTTLDays = 7;
constexpr int kSearchUserSegmentUnknownSelectionTTLDays = 7;

// List of sub-segments for Search User segment.
enum class SearchUserSubsegment {
  kUnknown = 0,
  kNone = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
  kMaxValue = kHigh
};

#define RANK(x) static_cast<int>(x)

// Discrete mapping parameters.
constexpr char kSearchUserDiscreteMappingKey[] = "search_user";

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

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
std::string SearchUserSubsegmentToString(SearchUserSubsegment subsegment) {
  switch (subsegment) {
    case SearchUserSubsegment::kUnknown:
      return "Unknown";
    case SearchUserSubsegment::kNone:
      return "None";
    case SearchUserSubsegment::kLow:
      return "Low";
    case SearchUserSubsegment::kMedium:
      return "Medium";
    case SearchUserSubsegment::kHigh:
      return "High";
  }
}

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
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformSearchUser,
          "segment_selection_ttl_days", kSearchUserSegmentSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformSearchUser,
          "unknown_selection_ttl_days",
          kSearchUserSegmentUnknownSelectionTTLDays));
  return config;
}

SearchUserModel::SearchUserModel() : ModelProvider(kSearchUserSegmentId) {}

absl::optional<std::string> SearchUserModel::GetSubsegmentName(
    int subsegment_rank) {
  DCHECK(RANK(SearchUserSubsegment::kUnknown) <= subsegment_rank &&
         subsegment_rank <= RANK(SearchUserSubsegment::kMaxValue));
  SearchUserSubsegment subgroup =
      static_cast<SearchUserSubsegment>(subsegment_rank);
  return SearchUserSubsegmentToString(subgroup);
}

void SearchUserModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata search_user_metadata;
  MetadataWriter writer(&search_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kSearchUserMinSignalCollectionLength, kSearchUserSignalStorageLength);

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMappingWithSubsegments(
      kSearchUserDiscreteMappingKey, RANK(SearchUserSubsegment::kMedium),
      RANK(SearchUserSubsegment::kMaxValue));

  // Set features.
  writer.AddUmaFeatures(kSearchUserUMAFeatures.data(),
                        kSearchUserUMAFeatures.size());

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

  float searches = inputs[0];

  SearchUserSubsegment segment;
  if (searches >= 22) {
    segment = SearchUserSubsegment::kHigh;
  } else if (searches >= 5) {
    segment = SearchUserSubsegment::kMedium;
  } else if (searches >= 1) {
    segment = SearchUserSubsegment::kLow;
  } else {
    segment = SearchUserSubsegment::kNone;
  }

  float result = RANK(segment);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

bool SearchUserModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
