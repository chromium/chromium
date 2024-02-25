// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/frequent_feature_user_model.h"

#include <array>

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for frequent feature user model.
constexpr SegmentId kFrequentFeatureUserSegmentId =
    SegmentId::FREQUENT_FEATURE_USER_SEGMENT;
constexpr int64_t kSignalStorageLength = 28;
constexpr int64_t kMinSignalCollectionLength = 7;

constexpr std::array<int32_t, 1> kUrlOnly{0};
constexpr std::array<int32_t, 1> kSearchOnly{1};

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 10> kUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuAddToBookmarks", 7),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuTranslate", 7),
    MetadataWriter::UMAFeature::FromUserAction("Suggestions.Content.Opened", 7),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuAllBookmarks", 7),
    MetadataWriter::UMAFeature::FromUserAction(
        "MobileBookmarkManagerEntryOpened",
        7),
    MetadataWriter::UMAFeature::FromUserAction(
        "Autofill.KeyMetrics.FillingAssistance.CreditCard",
        7),
    MetadataWriter::UMAFeature::FromUserAction("PasswordManager_Autofilled", 7),
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 7),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        7,
        kUrlOnly.data(),
        kUrlOnly.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        7,
        kSearchOnly.data(),
        kSearchOnly.size()),
};

}  // namespace

// static
std::unique_ptr<Config> FrequentFeatureUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kFrequentFeatureUserSegmentFeature)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kFrequentFeatureUserKey;
  config->segmentation_uma_name = kFrequentFeatureUserUmaName;
  config->AddSegmentId(SegmentId::FREQUENT_FEATURE_USER_SEGMENT,
                       std::make_unique<FrequentFeatureUserModel>());
  config->auto_execute_and_cache = true;
  config->is_boolean_segment = true;

  return config;
}

FrequentFeatureUserModel::FrequentFeatureUserModel()
    : DefaultModelProvider(kFrequentFeatureUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
FrequentFeatureUserModel::GetModelConfig() {
  proto::SegmentationModelMetadata frequent_feature_user_metadata;
  MetadataWriter writer(&frequent_feature_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  writer.AddOutputConfigForBinaryClassifier(
      0.5,
      SegmentIdToHistogramVariant(SegmentId::FREQUENT_FEATURE_USER_SEGMENT),
      kLegacyNegativeLabel);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/7, proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  constexpr int kModelVersion = 2;
  return std::make_unique<ModelConfig>(
      std::move(frequent_feature_user_metadata), kModelVersion);
}

void FrequentFeatureUserModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  int total_non_search_feature = 0;
  for (int i = 0; i <= 8; ++i)
    total_non_search_feature += inputs[i];

  float result = (total_non_search_feature > 0 && inputs[9] > 0) ? 1 : 0;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
