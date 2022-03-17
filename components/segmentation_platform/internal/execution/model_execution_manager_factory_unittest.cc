// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ModelExecutionManagerFactoryTest : public testing::Test {
 public:
  ModelExecutionManagerFactoryTest() = default;
  ~ModelExecutionManagerFactoryTest() override = default;

  void SetUp() override {
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    feature_list_query_processor_ = std::make_unique<FeatureListQueryProcessor>(
        &mock_signal_database_, std::make_unique<FeatureAggregatorImpl>());
    test_clock_.SetNow(base::Time::Now());
  }

  void TearDown() override {
    // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
    // to be destroyed.
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  MockSignalDatabase mock_signal_database_;
  std::unique_ptr<FeatureListQueryProcessor> feature_list_query_processor_;
};

TEST_F(ModelExecutionManagerFactoryTest, CreateModelExecutionManager) {
  TestModelProviderFactory::Data data;
  auto model_execution_manager = CreateModelExecutionManager(
      std::make_unique<TestModelProviderFactory>(&data),
      task_environment_.GetMainThreadTaskRunner(),
      {OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB},
      &test_clock_, segment_database_.get(), &mock_signal_database_,
      feature_list_query_processor_.get(), base::DoNothing());
  // This should work regardless of whether a DummyModelExecutionManager or
  // ModelExecutionManagerImpl is returned.
  ASSERT_TRUE(model_execution_manager);

  // Executing model with any provider should not crash.
  base::RunLoop wait_for_execution;
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  model_execution_manager->ExecuteModel(
      segment_info,
      base::BindOnce(
          [](base::RepeatingClosure quit,
             const std::pair<float, ModelExecutionStatus>&) { quit.Run(); },
          wait_for_execution.QuitClosure()));
  wait_for_execution.Run();
}

}  // namespace segmentation_platform
