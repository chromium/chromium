// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"

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

// Default parameters for shopping user model.
constexpr SegmentId kShoppingUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER;
constexpr int64_t kShoppingUserSignalStorageLength = 28;
constexpr int64_t kShoppingUserMinSignalCollectionLength = 0;
constexpr int64_t kModelVersion = 3;

// InputFeatures.

constexpr std::array<int32_t, 1> kProductDetailAvailableEnums{1};
constexpr std::array<float, 1> kShoppingUserDefaultValue{0};

constexpr std::array<MetadataWriter::UMAFeature, 3> kShoppingUserUMAFeatures = {
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Commerce.PriceDrops.ActiveTabNavigationComplete.IsProductDetailPage",
        7,
        kProductDetailAvailableEnums.data(),
        kProductDetailAvailableEnums.size()),
    MetadataWriter::UMAFeature::FromUserAction(
        "Autofill_PolledCreditCardSuggestions",
        7),
    MetadataWriter::UMAFeature::FromValueHistogram(
        "IOS.ParcelTracking.Tracked.AutoTrack",
        7,
        proto::Aggregation::LATEST_OR_DEFAULT,
        kShoppingUserDefaultValue.size(),
        kShoppingUserDefaultValue.data())};
}  // namespace

// static
std::unique_ptr<Config> ShoppingUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(features::kShoppingUserSegmentFeature)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kShoppingUserSegmentationKey;
  config->segmentation_uma_name = kShoppingUserUmaName;
  config->auto_execute_and_cache = true;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER,
      std::make_unique<ShoppingUserModel>());
  config->is_boolean_segment = true;
  return config;
}

ShoppingUserModel::ShoppingUserModel()
    : DefaultModelProvider(kShoppingUserSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
ShoppingUserModel::GetModelConfig() {
  proto::SegmentationModelMetadata shopping_user_metadata;
  MetadataWriter writer(&shopping_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kShoppingUserMinSignalCollectionLength, kShoppingUserSignalStorageLength);

  // Set features.
  writer.AddUmaFeatures(kShoppingUserUMAFeatures.data(),
                        kShoppingUserUMAFeatures.size());

  // Adding custom inputs.
  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_FROM_SHOPPING_SERVICE,
      .name = "TotalShoppingBookmarkCount"});

  // Set OutputConfig.
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5f,
      /*positive_label=*/kShoppingUserUmaName,
      /*negative_label=*/kLegacyNegativeLabel);

  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/2,
      /*time_unit=*/proto::TimeUnit::DAY);

  return std::make_unique<ModelConfig>(std::move(shopping_user_metadata),
                                       kModelVersion);
}

void ShoppingUserModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kShoppingUserUMAFeatures.size() + 1 /*custom_inputs*/) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  float result = 0;

  // Determine if the user is a shopping user using  price tracking, price drop,
  // product detail page info, etc. features count.
  if (inputs[0] > 1 || inputs[1] > 1 || inputs[2] > 0 || inputs[3] > 0) {
    result = 1;  // User classified as shopping user;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
