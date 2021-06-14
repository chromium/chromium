// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/database/user_action_database.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
class ModelExecutionManagerTest : public testing::Test {
 public:
  ModelExecutionManagerTest() = default;
  ~ModelExecutionManagerTest() override = default;

  void SetUp() override {
    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
  }

  void TearDown() override {
    model_execution_manager_.reset();
    // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
    // to be destroyed.
    RunUntilIdle();
  }

  void CreateModelExecutionManager(
      std::vector<OptimizationTarget> segment_ids) {
    model_execution_manager_ = std::make_unique<ModelExecutionManagerImpl>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(), segment_ids,
        segment_database_.get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExecuteModel(const std::pair<float, ModelExecutionStatus>& expected) {
    base::RunLoop loop;
    model_execution_manager_->ExecuteModel(
        OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
        base::BindOnce(&ModelExecutionManagerTest::OnExecutionCallback,
                       base::Unretained(this), loop.QuitClosure(), expected));
    loop.Run();
  }

  void OnExecutionCallback(
      base::RepeatingClosure closure,
      const std::pair<float, ModelExecutionStatus>& expected,
      const std::pair<float, ModelExecutionStatus>& actual) {
    EXPECT_EQ(expected.second, actual.second);
    EXPECT_NEAR(expected.first, actual.first, 1e-5);
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;

  std::unique_ptr<ModelExecutionManagerImpl> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, HandlerNotRegistered) {
  CreateModelExecutionManager({});
  EXPECT_DCHECK_DEATH(
      ExecuteModel(std::make_pair(0, ModelExecutionStatus::EXECUTION_ERROR)));
}

TEST_F(ModelExecutionManagerTest, MetadataTests) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id});
  ExecuteModel(std::make_pair(0, ModelExecutionStatus::INVALID_METADATA));

  segment_database_->SetBucketDuration(segment_id, 14,
                                       proto::TimeUnit::UNKNOWN_TIME_UNIT);
  ExecuteModel(std::make_pair(0, ModelExecutionStatus::INVALID_METADATA));
}

TEST_F(ModelExecutionManagerTest, SingleUserAction) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id});
  std::string kUserActionName1 = "some_action_1";
  segment_database_->AddUserActionFeature(segment_id, kUserActionName1, 0,
                                          proto::Aggregation::SUM_COUNT);
  segment_database_->SetBucketDuration(segment_id, 14, proto::TimeUnit::DAY);
  ExecuteModel(std::make_pair(1, ModelExecutionStatus::SUCCESS));
}

}  // namespace segmentation_platform
