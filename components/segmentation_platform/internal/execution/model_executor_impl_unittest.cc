// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_executor_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/execution/model_executor.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
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

const SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

class ModelExecutorTest : public testing::Test {
 public:
  ModelExecutorTest() : mock_model_(kSegmentId, base::DoNothing()) {}
  ~ModelExecutorTest() override = default;

  void SetUp() override {
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    signal_database_ = std::make_unique<MockSignalDatabase>();
    clock_.SetNow(base::Time::Now());
  }

  void TearDown() override {
    model_executor_.reset();
    // Allow for the SegmentationModelExecutor owned by ModelProvider
    // to be destroyed.
    RunUntilIdle();
    segment_database_.reset();
    signal_database_.reset();
  }

  void CreateModelExecutor() {
    feature_list_query_processor_ =
        std::make_unique<processing::MockFeatureListQueryProcessor>();
    model_executor_ = std::make_unique<ModelExecutorImpl>(
        &clock_, segment_database_.get(), feature_list_query_processor_.get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExecuteModel(const proto::SegmentInfo& info,
                    ModelProvider* model,
                    std::unique_ptr<ModelExecutionResult> expected) {
    segment_database_->UpdateSegment(info.segment_id(), info.model_source(),
                                     info, base::DoNothing());
    base::RunLoop loop;
    auto request = std::make_unique<ExecutionRequest>();
    request->segment_id = info.segment_id();
    request->model_source = info.model_source();
    request->model_provider = model;
    request->save_result_to_db = false;
    request->callback = base::BindOnce(&ModelExecutorTest::OnExecutionCallback,
                                       base::Unretained(this),
                                       loop.QuitClosure(), std::move(expected));
    model_executor_->ExecuteModel(std::move(request));
    loop.Run();
  }

  void OnExecutionCallback(base::RepeatingClosure closure,
                           std::unique_ptr<ModelExecutionResult> expected,
                           std::unique_ptr<ModelExecutionResult> actual) {
    EXPECT_EQ(expected->status, actual->status);
    EXPECT_EQ(expected->scores, actual->scores);
    EXPECT_EQ(expected->inputs, actual->inputs);
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  MockModelProvider mock_model_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<MockSignalDatabase> signal_database_;

  std::unique_ptr<processing::MockFeatureListQueryProcessor>
      feature_list_query_processor_;
  std::unique_ptr<ModelExecutorImpl> model_executor_;
};

TEST_F(ModelExecutorTest, MetadataTests) {
  CreateModelExecutor();
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(kSegmentId);
  segment_info.set_model_source(ModelSource::SERVER_MODEL_SOURCE);

  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(true));
  ExecuteModel(segment_info, &mock_model_,
               std::make_unique<ModelExecutionResult>(
                   ModelExecutionStatus::kSkippedInvalidMetadata));

  auto& model_metadata = *segment_info.mutable_model_metadata();
  model_metadata.set_bucket_duration(14);
  model_metadata.set_time_unit(proto::TimeUnit::UNKNOWN_TIME_UNIT);

  ExecuteModel(segment_info, &mock_model_,
               std::make_unique<ModelExecutionResult>(
                   ModelExecutionStatus::kSkippedInvalidMetadata));
}

TEST_F(ModelExecutorTest, ModelNotReady) {
  CreateModelExecutor();

  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(kSegmentId);
  segment_info.set_model_source(ModelSource::SERVER_MODEL_SOURCE);
  auto& model_metadata = *segment_info.mutable_model_metadata();
  model_metadata.set_bucket_duration(14);
  model_metadata.set_time_unit(proto::TimeUnit::UNKNOWN_TIME_UNIT);

  // When the model is unavailable, the execution should fail.
  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(false));

  ExecuteModel(segment_info, &mock_model_,
               std::make_unique<ModelExecutionResult>(
                   ModelExecutionStatus::kSkippedModelNotReady));
}

TEST_F(ModelExecutorTest, FailedFeatureProcessing) {
  CreateModelExecutor();

  // Initialize with required metadata.
  const SegmentId segment_id = kSegmentId;
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  std::string user_action_name = "some_user_action";
  segment_database_->AddUserActionFeature(segment_id, user_action_name, 3, 3,
                                          proto::Aggregation::BUCKETED_COUNT);

  EXPECT_CALL(*feature_list_query_processor_,
              ProcessFeatureList(
                  _, _, segment_id, clock_.Now(), base::Time(),
                  FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/true,
                                   ModelProvider::Request{1, 2, 3},
                                   ModelProvider::Response()));

  // The input tensor should contain all values flattened to a single vector.
  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_model_, ExecuteModelWithInput(_, _)).Times(0);

  ExecuteModel(*segment_database_->FindOrCreateSegment(segment_id),
               &mock_model_,
               std::make_unique<ModelExecutionResult>(
                   ModelExecutionStatus::kSkippedInvalidMetadata));

  EXPECT_CALL(*feature_list_query_processor_,
              ProcessFeatureList(
                  _, _, segment_id, clock_.Now(), base::Time(),
                  FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/true, ModelProvider::Request(),
                                   ModelProvider::Response()));
  ExecuteModel(*segment_database_->FindOrCreateSegment(segment_id),
               &mock_model_,
               std::make_unique<ModelExecutionResult>(
                   ModelExecutionStatus::kSkippedInvalidMetadata));
}

TEST_F(ModelExecutorTest, ExecuteModelWithMultipleFeatures) {
  CreateModelExecutor();

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(kSegmentId, 3, proto::TimeUnit::HOUR);
  std::string user_action_name = "some_user_action";
  segment_database_->AddUserActionFeature(kSegmentId, user_action_name, 3, 3,
                                          proto::Aggregation::BUCKETED_COUNT);
  const ModelProvider::Request inputs{1, 2, 3, 4, 5, 6, 7};

  EXPECT_CALL(*feature_list_query_processor_,
              ProcessFeatureList(
                  _, _, kSegmentId, clock_.Now(), base::Time(),
                  FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/false, inputs,
                                   ModelProvider::Response()));

  // The input tensor should contain all values flattened to a single vector.
  EXPECT_CALL(mock_model_, ModelAvailable()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_model_, ExecuteModelWithInput(inputs, _))
      .WillOnce(
          RunOnceCallback<1>(std::optional<ModelProvider::Response>{{0.8}}));

  ExecuteModel(
      *segment_database_->FindOrCreateSegment(kSegmentId), &mock_model_,
      std::make_unique<ModelExecutionResult>(ModelProvider::Request(inputs),
                                             ModelProvider::Response(1, 0.8)));
}

}  // namespace segmentation_platform
