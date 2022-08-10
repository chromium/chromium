// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"

#include <array>

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for shopping user model.
constexpr SegmentId kShoppingUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER;
constexpr proto::TimeUnit kShoppingUserTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kShoppingUserBucketDuration = 1;
constexpr int64_t kShoppingUserSignalStorageLength = 28;
constexpr int64_t kShoppingUserMinSignalCollectionLength = 1;
constexpr int64_t kShoppingUserResultTTL = 1;
constexpr int64_t kModelVersion = 1;

// Discrete mapping parameters.
constexpr char kShoppingUserDiscreteMappingKey[] = "shopping_user";
constexpr float kShoppingUserDiscreteMappingMinResult = 1;
constexpr int64_t kShoppingUserDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kShoppingUserDiscreteMappingMinResult, kShoppingUserDiscreteMappingRank}};

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kShoppingUserUMAFeatures = {
    MetadataWriter::UMAFeature::FromValueHistogram("NewTabPage.Carts.CartCount",
                                                   7,
                                                   proto::Aggregation::SUM),
};
}  // namespace

ShoppingUserModel::ShoppingUserModel()
    : ModelProvider(kShoppingUserSegmentId) {}

void ShoppingUserModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata shopping_user_metadata;
  MetadataWriter writer(&shopping_user_metadata);
  writer.SetSegmentationMetadataConfig(
      kShoppingUserTimeUnit, kShoppingUserBucketDuration,
      kShoppingUserSignalStorageLength, kShoppingUserMinSignalCollectionLength,
      kShoppingUserResultTTL);

  // Set discrete mapping.
  writer.AddDiscreteMappingEntries(kShoppingUserDiscreteMappingKey,
                                   kDiscreteMappings, 1);

  // Set features.
  writer.AddUmaFeatures(kShoppingUserUMAFeatures.data(),
                        kShoppingUserUMAFeatures.size());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kShoppingUserSegmentId,
                          std::move(shopping_user_metadata), kModelVersion));
}

void ShoppingUserModel::ExecuteModelWithInput(const std::vector<float>& inputs,
                                              ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kShoppingUserUMAFeatures.size()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  const int new_tab_page_cart_count = inputs[0];
  float result = 0;

  // A user is considered as a shopping user, if
  //  1. Cart count in new tab page greater than 0
  if (new_tab_page_cart_count > 0) {
    result = 1;  // User classified as shopping user;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool ShoppingUserModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
