// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/intentional_user_model.h"

#include <array>

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

// Default parameters for intentional user model.
constexpr SegmentId kIntentionalUserSegmentId =
    SegmentId::INTENTIONAL_USER_SEGMENT;
// Store 28 buckets of input data (28 days).
constexpr int64_t kIntentionalUserSignalStorageLength = 28;
// Wait until we have 7 days of data.
constexpr int64_t kIntentionalUserMinSignalCollectionLength = 7;
// Refresh result every 7 days.
constexpr int64_t kIntentionalUserResultTTLDays = 7;
// Threshold for our heuristic, if the user launched Chrome directly at least 2
// times in the last 28 days then we consider them an intentional user.
constexpr int64_t kIntentionalLaunchThreshold = 2;

constexpr int64_t kIntentionalUserModelVersion = 2;

// InputFeatures.

// MobileStartup.LaunchCause enum values to record an aggregate, these values
// come from LaunchCauseMetrics.LaunchCause.
constexpr std::array<int32_t, 1> kLaunchCauseMainLauncherIcon{
    6  // MAIN_LAUNCHER_ICON.
};

// Set UMA metrics to use as input.
constexpr std::array<MetadataWriter::UMAFeature, 1>
    kIntentionalUserUMAFeatures = {
        // This input is the sum of all times MobileStartup.LaunchCause was
        // recorded with a value of MAIN_LAUNCHER_ICON in the last 28 days.
        MetadataWriter::UMAFeature::FromEnumHistogram(
            "MobileStartup.LaunchCause",
            /* This is the number of buckets to store and aggregate, each bucket
               is 1 day according to kIntentionalUserTimeUnit and
               kIntentionalUserBucketDuration. */
            28,
            kLaunchCauseMainLauncherIcon.data(),
            kLaunchCauseMainLauncherIcon.size())};

}  // namespace

// static
std::unique_ptr<Config> IntentionalUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformIntentionalUser)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kIntentionalUserKey;
  config->segmentation_uma_name = kIntentionalUserUmaName;
  config->AddSegmentId(SegmentId::INTENTIONAL_USER_SEGMENT,
                       std::make_unique<IntentionalUserModel>());
  config->auto_execute_and_cache = true;
  config->is_boolean_segment = true;

  return config;
}

IntentionalUserModel::IntentionalUserModel()
    : DefaultModelProvider(kIntentionalUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
IntentionalUserModel::GetModelConfig() {
  proto::SegmentationModelMetadata intentional_user_metadata;
  MetadataWriter writer(&intentional_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kIntentionalUserMinSignalCollectionLength,
      kIntentionalUserSignalStorageLength);

  // If the result from ExecuteModelWithInput is greater than 0.5 then return
  // the intentional user label, otherwise return the non-intentional label.
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5f, /*positive_label=*/
      SegmentIdToHistogramVariant(SegmentId::INTENTIONAL_USER_SEGMENT),
      /*negative_label=*/kLegacyNegativeLabel);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kIntentionalUserResultTTLDays,
      /*time_unit=*/proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kIntentionalUserUMAFeatures.data(),
                        kIntentionalUserUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(intentional_user_metadata),
                                       kIntentionalUserModelVersion);
}

void IntentionalUserModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kIntentionalUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  const int main_launcher_clicks = inputs[0];
  float result = 0;

  if (main_launcher_clicks >= kIntentionalLaunchThreshold) {
    result = 1;  // User is intentionally using Chrome.
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
