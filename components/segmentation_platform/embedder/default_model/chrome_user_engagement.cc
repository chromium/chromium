// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/chrome_user_engagement.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

BASE_FEATURE(kSegmentationPlatformChromeUserEngagement,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

using proto::SegmentId;

// Default parameters for ChromeUserEngagement model.
constexpr SegmentId kSegmentId = SegmentId::CHROME_USER_ENGAGEMENT;
constexpr int64_t kModelVersion = 1;
constexpr int64_t kSignalStorageLength = 28;
constexpr int64_t kMinSignalCollectionLength = 28;
constexpr int64_t kResultTTLDays = 1;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kUmaFeatures = {
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_VALUE,
        .name = "Session.TotalDuration",
        .bucket_count = 28,
        .tensor_length = 28,
        .aggregation = proto::Aggregation::BUCKETED_COUNT,
        .enum_ids_size = 0}};

// List of user engagement segments.
enum class UserEngagementBucket {
  kUnknown = 0,

  kNone = 1,
  kOneDay = 2,
  kLow = 3,
  kMedium = 4,
  kPower = 5,
  kMaxValue = kPower
};

#define RANK(x) static_cast<int>(x)

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
std::string UserEngagementBucketToString(UserEngagementBucket power_group) {
  switch (power_group) {
    case UserEngagementBucket::kUnknown:
      return "Unknown";
    case UserEngagementBucket::kNone:
      return "None";
    case UserEngagementBucket::kOneDay:
      return "OneDay";
    case UserEngagementBucket::kLow:
      return "Low";
    case UserEngagementBucket::kMedium:
      return "Medium";
    case UserEngagementBucket::kPower:
      return "Power";
  }
}

}  // namespace

// static
std::unique_ptr<Config> ChromeUserEngagement::GetConfig() {
  if (!base::FeatureList::IsEnabled(
          kSegmentationPlatformChromeUserEngagement)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeUserEngagementKey;
  config->segmentation_uma_name = kChromeUserEngagementUmaName;
  config->AddSegmentId(kSegmentId, std::make_unique<ChromeUserEngagement>());
  config->auto_execute_and_cache = true;
  return config;
}

ChromeUserEngagement::ChromeUserEngagement()
    : DefaultModelProvider(kSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
ChromeUserEngagement::GetModelConfig() {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(kMinSignalCollectionLength,
                                              kSignalStorageLength);

  // Set output config.
  static_assert(static_cast<int>(UserEngagementBucket::kMaxValue) == 5,
                "Please update output config when updating the bins");

  writer.AddOutputConfigForBinnedClassifier(
      {
          /*bins=*/{
              {RANK(UserEngagementBucket::kNone),
               UserEngagementBucketToString(UserEngagementBucket::kNone)},
              {RANK(UserEngagementBucket::kOneDay),
               UserEngagementBucketToString(UserEngagementBucket::kOneDay)},
              {RANK(UserEngagementBucket::kLow),
               UserEngagementBucketToString(UserEngagementBucket::kLow)},
              {RANK(UserEngagementBucket::kMedium),
               UserEngagementBucketToString(UserEngagementBucket::kMedium)},
              {RANK(UserEngagementBucket::kPower),
               UserEngagementBucketToString(UserEngagementBucket::kPower)}},
      },
      /*underflow_label=*/"Unknown");
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/kResultTTLDays,
      /*time_unit=*/proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kUmaFeatures.data(), kUmaFeatures.size());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}

void ChromeUserEngagement::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 28) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // One-day: 1 day active out of the last 28 days
  // Low: 2 - 8 days active
  // Medium: 9-16 days active
  // Power: 17+ days active
  int days_active = 0;
  for (unsigned i = 0; i < 28; ++i) {
    days_active += inputs[i] > 0 ? 1 : 0;
  }

  UserEngagementBucket segment = UserEngagementBucket::kNone;
  if (days_active >= 17) {
    segment = UserEngagementBucket::kPower;
  } else if (days_active >= 9) {
    segment = UserEngagementBucket::kMedium;
  } else if (days_active >= 2) {
    segment = UserEngagementBucket::kLow;
  } else if (days_active == 1) {
    segment = UserEngagementBucket::kOneDay;
  }
  float result = RANK(segment);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
