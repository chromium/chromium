// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {
namespace {
constexpr auto kTestOptimizationTarget =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
}  // namespace

class MockModelExecutionObserver : public ModelExecutionScheduler::Observer {
 public:
  MockModelExecutionObserver() = default;
  MOCK_METHOD(void, OnModelExecutionCompleted, (OptimizationTarget));
};

class MockModelExecutionManager : public ModelExecutionManager {
 public:
  MockModelExecutionManager() = default;
  MOCK_METHOD(void, ExecuteModel, (OptimizationTarget, ModelExecutionCallback));
};

class MockSignalStorageConfig : public SignalStorageConfig {
 public:
  MockSignalStorageConfig() : SignalStorageConfig(nullptr, nullptr) {}
  MOCK_METHOD(bool,
              MeetsSignalCollectionRequirement,
              (const proto::SegmentationModelMetadata&),
              (override));
};

class ModelExecutionSchedulerTest : public testing::Test {
 public:
  ModelExecutionSchedulerTest() = default;
  ~ModelExecutionSchedulerTest() override = default;

  void SetUp() override {
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
        &observer_, segment_database_.get(), &signal_storage_config_,
        &model_execution_manager_);
  }

  base::test::TaskEnvironment task_environment_;
  MockModelExecutionObserver observer_;
  MockSignalStorageConfig signal_storage_config_;
  MockModelExecutionManager model_execution_manager_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<ModelExecutionScheduler> model_execution_scheduler_;
};

TEST_F(ModelExecutionSchedulerTest, OnNewModelInfoReady) {
  segment_database_->FindOrCreateSegment(kTestOptimizationTarget);

  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(1);
  model_execution_scheduler_->OnNewModelInfoReady(kTestOptimizationTarget);
}

TEST_F(ModelExecutionSchedulerTest, RequestModelExecutionForEligibleSegments) {
  segment_database_->FindOrCreateSegment(kTestOptimizationTarget);

  // TODO(shaktisahu): Add tests for expired segments, freshly computed segments
  // etc.

  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(1);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));
  // TODO(shaktisahu): Add test when the signal collection returns false.

  model_execution_scheduler_->RequestModelExecutionForEligibleSegments(true);
}

TEST_F(ModelExecutionSchedulerTest, OnModelExecutionCompleted) {
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(kTestOptimizationTarget);

  // TODO(shaktisahu): Add tests for model failure.
  EXPECT_CALL(observer_, OnModelExecutionCompleted(kTestOptimizationTarget))
      .Times(1);
  float score = 0.4;
  model_execution_scheduler_->OnModelExecutionCompleted(
      kTestOptimizationTarget,
      std::make_pair(score, ModelExecutionStatus::SUCCESS));

  // Verify that the results are written to the DB.
  segment_info =
      segment_database_->FindOrCreateSegment(kTestOptimizationTarget);
  ASSERT_TRUE(segment_info->has_prediction_result());
  ASSERT_EQ(score, segment_info->prediction_result().result());
}

}  // namespace segmentation_platform
