// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/fedcm_user_segment.h"

#include <cstdint>
#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for FedCM user model.
constexpr SegmentId kFedCmUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEDCM_USER;
// Store 28 buckets of input data (28 days).
constexpr int64_t kFedCmUserSignalStorageLength = 28;
// Collect signals immediately.
constexpr int64_t kFedCmUserMinSignalCollectionLength = 0;
// Refresh the result every 7 days.
constexpr int64_t kFedCmUserResultTTLDays = 7;

// InputFeatures.

// Positive enum value for Blink.FedCm.Status.RequestIdToken.
constexpr std::array<int32_t, 2> kTokenResponsePositiveEnumValues{
    0,   // Success using token in HTTP response.
    46,  // Success using identity provider resolve.
};

// Negative enum values for Blink.FedCm.CancelReason.
constexpr std::array<int32_t, 2> kCancelReasonNegativeEnumValues{
    2,  // Close button.
    3,  // Swipe.
};

// Enum value for Blink.FedCm.IsSignInUser.
constexpr std::array<int32_t, 1> kIsSignInUserValue{
    1,  // True.
};

const char kFedCmUserLoudLabel[] = "FedCmUserLoud";
const char kFedCmUserQuietLabel[] = "FedCmUserQuiet";

// Set UMA metrics to use as input.
constexpr FeaturePair<FedCmUserModel::Feature> kFedCmUserUMAFeatures[] = {
    // Number of times accounts dialog is shown.
    {FedCmUserModel::kFeatureAccountsDialogShown,
     features::UMACount("Blink.FedCm.AccountsDialogShown", 28)},
    // Number of successful token requests.
    {FedCmUserModel::kFeatureRequestIdToken,
     features::UMAEnum("Blink.FedCm.Status.RequestIdToken",
                       28,
                       kTokenResponsePositiveEnumValues)},
    // Number of times user has intentionally closed FedCM UI. (Close
    // button/Swipe)
    {FedCmUserModel::kFeatureCancelReason,
     features::UMAEnum("Blink.FedCm.CancelReason",
                       28,
                       kCancelReasonNegativeEnumValues)},
    // Whether the user is signed in i.e. used FedCM previously.
    {FedCmUserModel::kFeatureIsSignInUser,
     features::UMAEnum("Blink.FedCm.IsSignInUser", 28, kIsSignInUserValue)},
};
}  // namespace

// static
std::unique_ptr<Config> FedCmUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(features::kSegmentationPlatformFedCmUser)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kFedCmUserKey;
  config->segmentation_uma_name = kFedCmUserUmaName;
  config->AddSegmentId(kFedCmUserSegmentId, std::make_unique<FedCmUserModel>());
  config->auto_execute_and_cache = false;
  config->is_boolean_segment = true;

  return config;
}

FedCmUserModel::FedCmUserModel() : DefaultModelProvider(kFedCmUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
FedCmUserModel::GetModelConfig() {
  proto::SegmentationModelMetadata intentional_user_metadata;
  MetadataWriter writer(&intentional_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kFedCmUserMinSignalCollectionLength, kFedCmUserSignalStorageLength);

  // Set output config.
  writer.AddOutputConfigForBinaryClassifier(
      0.5,
      /*positive_label=*/kFedCmUserLoudLabel,
      /*negative_label=*/kFedCmUserQuietLabel);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{},
      /*default_ttl=*/kFedCmUserResultTTLDays, proto::TimeUnit::DAY);

  // Set features.
  writer.AddFeatures<Feature>(kFedCmUserUMAFeatures);

  return std::make_unique<ModelConfig>(std::move(intentional_user_metadata),
                                       /*model_version=*/1);
}

void FedCmUserModel::ExecuteModelWithInput(const ModelProvider::Request& inputs,
                                           ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kFeatureCount) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  float result = 1;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
