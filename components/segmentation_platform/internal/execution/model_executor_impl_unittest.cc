// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_executor_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/execution/model_executor.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using segmentation_platform::processing::FeatureListQueryProcessor;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::SetArgReferee;

namespace segmentation_platform {

const OptimizationTarget kSegmentId =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

class ModelExecutorTest : public testing::Test {
 public:
  ModelExecutorTest() : mock_model_(kSegmentId, base::DoNothing()) {}
  ~ModelExecutorTest() override = default;

  void SetUp() override {
    signal_database_ = std::make_unique<MockSignalDatabase>();
    clock_.SetNow(base::Time::Now());
  }

  void TearDown() override {
    model_executor_.reset();
    // Allow for the SegmentationModelExecutor owned by ModelProvider
    // to be destroyed.
    RunUntilIdle();
  }

  void CreateModelExecutor() {
    feature_list_query_processor_ =
        std::make_unique<processing::MockFeatureListQueryProcessor>();
    model_executor_ = std::make_unique<ModelExecutorImpl>(
        &clock_, feature_list_query_processor_.get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExecuteModel(const proto::SegmentInfo& info,
                    ModelProvider* model,
                    const std::pair<float, ModelExecutionStatus>& expected) {
    base::RunLoop loop;
    model_executor_->ExecuteModel(
        info, model, /*record_metrics_for_default=*/false,
        base::BindOnce(&ModelExecutorTest::OnExecutionCallback,
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

 protected:
  base::test::TaskEnvironment task_environment_;

  MockModelProvider mock_model_;
  base::SimpleTestClock clock_;
  std::unique_ptr<MockSignalDatabase> signal_database_;

  std::unique_ptr<processing::MockFeatureListQueryProcessor>
      feature_list_query_processor_;
  std::unique_ptr<ModelExecutorImpl> model_executor_;
};

TEST_F(ModelExecutorTest, MetadataTests) {
  CreateModelExecutor();
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(kSegmentId);

  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(true));
  ExecuteModel(
      segment_info, &mock_model_,
      std::make_pair(0, ModelExecutionStatus::kSkippedInvalidMetadata));

  auto& model_metadata = *segment_info.mutable_model_metadata();
  model_metadata.set_bucket_duration(14);
  model_metadata.set_time_unit(proto::TimeUnit::UNKNOWN_TIME_UNIT);

  ExecuteModel(
      segment_info, &mock_model_,
      std::make_pair(0, ModelExecutionStatus::kSkippedInvalidMetadata));
}

TEST_F(ModelExecutorTest, ModelNotReady) {
  CreateModelExecutor();

  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(kSegmentId);
  auto& model_metadata = *segment_info.mutable_model_metadata();
  model_metadata.set_bucket_duration(14);
  model_metadata.set_time_unit(proto::TimeUnit::UNKNOWN_TIME_UNIT);

  // When the model is unavailable, the execution should fail.
  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(false));

  ExecuteModel(segment_info, &mock_model_,
               std::make_pair(0, ModelExecutionStatus::kSkippedModelNotReady));
}

TEST_F(ModelExecutorTest, FailedFeatureProcessing) {
  CreateModelExecutor();

  // Initialize with required metadata.
  test::TestSegmentInfoDatabase metadata_writer;
  const OptimizationTarget segment_id = kSegmentId;
  metadata_writer.SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  std::string user_action_name = "some_user_action";
  metadata_writer.AddUserActionFeature(segment_id, user_action_name, 3, 3,
                                       proto::Aggregation::BUCKETED_COUNT);

  EXPECT_CALL(*feature_list_query_processor_,
              ProcessFeatureList(
                  _, segment_id, clock_.Now(),
                  FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<4>(/*error=*/true, std::vector<float>{1, 2, 3},
                                   std::vector<float>()));

  // The input tensor should contain all values flattened to a single vector.
  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_model_, ExecuteModelWithInput(_, _)).Times(0);

  ExecuteModel(
      *metadata_writer.FindOrCreateSegment(segment_id), &mock_model_,
      std::make_pair(0, ModelExecutionStatus::kSkippedInvalidMetadata));

  EXPECT_CALL(*feature_list_query_processor_,
              ProcessFeatureList(
                  _, segment_id, clock_.Now(),
                  FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<4>(/*error=*/true, std::vector<float>(),
                                   std::vector<float>()));
  ExecuteModel(
      *metadata_writer.FindOrCreateSegment(segment_id), &mock_model_,
      std::make_pair(0, ModelExecutionStatus::kSkippedInvalidMetadata));
}

TEST_F(ModelExecutorTest, ExecuteModelWithMultipleFeatures) {
  CreateModelExecutor();

  // Initialize with required metadata.
  test::TestSegmentInfoDatabase metadata_writer;
  metadata_writer.SetBucketDuration(kSegmentId, 3, proto::TimeUnit::HOUR);
  std::string user_action_name = "some_user_action";
  metadata_writer.AddUserActionFeature(kSegmentId, user_action_name, 3, 3,
                                       proto::Aggregation::BUCKETED_COUNT);

  EXPECT_CALL(*feature_list_query_processor_,
              ProcessFeatureList(
                  _, kSegmentId, clock_.Now(),
                  FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<4>(/*error=*/false,
                                   std::vector<float>{1, 2, 3, 4, 5, 6, 7},
                                   std::vector<float>()));

  // The input tensor should contain all values flattened to a single vector.
  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_model_,
              ExecuteModelWithInput(std::vector<float>{1, 2, 3, 4, 5, 6, 7}, _))
      .WillOnce(RunOnceCallback<1>(absl::make_optional(0.8)));

  ExecuteModel(*metadata_writer.FindOrCreateSegment(kSegmentId), &mock_model_,
               std::make_pair(0.8, ModelExecutionStatus::kSuccess));
}

}  // namespace segmentation_platform
