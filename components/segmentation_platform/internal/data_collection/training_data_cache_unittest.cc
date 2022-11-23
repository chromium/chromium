// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

// Test Ids.
const proto::SegmentId kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

const TrainingDataCache::RequestId kRequestId =
    TrainingDataCache::RequestId::FromUnsafeValue(1);

}  // namespace

class TrainingDataCacheTest : public testing::Test {
 public:
  TrainingDataCacheTest() = default;
  ~TrainingDataCacheTest() override = default;

 protected:
  void SetUp() override {
    DCHECK(!training_data_cache_);
    training_data_cache_ = std::make_unique<TrainingDataCache>();
  }

  void TearDown() override { training_data_cache_.reset(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TrainingDataCache> training_data_cache_;
};

TEST_F(TrainingDataCacheTest, GetTrainingDataFromEmptyCache) {
  auto training_data =
      training_data_cache_->GetInputsAndDelete(kSegmentId, kRequestId);
  EXPECT_FALSE(training_data.has_value());
}

TEST_F(TrainingDataCacheTest, GetTrainingDataFromCache) {
  ModelProvider::Request data = {1, 2, 3};
  training_data_cache_->StoreInputs(kSegmentId, kRequestId, data);
  auto training_data =
      training_data_cache_->GetInputsAndDelete(kSegmentId, kRequestId);
  EXPECT_TRUE(training_data.has_value());
  for (int i = 0; i < training_data.value().inputs_size(); i++) {
    EXPECT_EQ(data[i], training_data.value().inputs(i));
  }
}

}  // namespace segmentation_platform
