// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_manager.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {
using SignalType = proto::SignalType;
using SignalIdentifier = std::pair<uint64_t, SignalType>;

namespace {
constexpr auto kTestSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
constexpr auto kTestOptimizationTarget2 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY;
}  // namespace

class MockModelExecutionObserver : public ModelExecutionScheduler::Observer {
 public:
  MockModelExecutionObserver() = default;
  MOCK_METHOD(void, OnModelExecutionCompleted, (SegmentId));
};

class MockModelManager : public ModelManager {
 public:
  MOCK_METHOD(ModelProvider*,
              GetModelProvider,
              (proto::SegmentId segment_id, proto::ModelSource model_source));
  MOCK_METHOD(void, Initialize, ());
  MOCK_METHOD(
      void,
      SetSegmentationModelUpdatedCallbackForTesting,
      (ModelManager::SegmentationModelUpdatedCallback model_updated_callback));
};

class MockModelExecutor : public ModelExecutor {
 public:
  MockModelExecutor() = default;
  MOCK_METHOD(void, ExecuteModel, (std::unique_ptr<ExecutionRequest>));
};

class ModelExecutionSchedulerTest : public testing::Test {
 public:
  ModelExecutionSchedulerTest() = default;
  ~ModelExecutionSchedulerTest() override = default;

  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    std::vector<raw_ptr<ModelExecutionScheduler::Observer, VectorExperimental>>
        observers = {&observer1_, &observer2_};
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    base::flat_set<SegmentId> segment_ids;
    segment_ids.insert(kTestSegmentId);
    model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
        std::move(observers), segment_database_.get(), &signal_storage_config_,
        &model_manager_, &model_executor_, segment_ids, &clock_,
        PlatformOptions::CreateDefault());
  }

  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  MockModelExecutionObserver observer1_;
  MockModelExecutionObserver observer2_;
  MockSignalStorageConfig signal_storage_config_;
  MockModelManager model_manager_;
  MockModelExecutor model_executor_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<ModelExecutionScheduler> model_execution_scheduler_;
};

MATCHER_P(IsForTarget, segment_id, "") {
  return arg->segment_id == segment_id;
}

TEST_F(ModelExecutionSchedulerTest, OnNewModelInfoReady) {
  auto* segment_info = segment_database_->FindOrCreateSegment(kTestSegmentId);
  segment_info->set_segment_id(kTestSegmentId);
  segment_info->set_model_source(proto::ModelSource::SERVER_MODEL_SOURCE);
  auto* metadata = segment_info->mutable_model_metadata();
  metadata->set_result_time_to_live(1);
  metadata->set_time_unit(proto::TimeUnit::DAY);
  MockModelProvider provider(kTestSegmentId, base::DoNothing());

  // If the metadata DOES NOT meet the signal requirement, we SHOULD NOT try to
  // execute the model.
  EXPECT_CALL(model_executor_, ExecuteModel(_)).Times(0);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(false));
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If the metadata DOES meet the signal requirement, and we have no old,
  // PredictionResult we SHOULD try to execute the model.
  EXPECT_CALL(
      model_manager_,
      GetModelProvider(kTestSegmentId, proto::ModelSource::SERVER_MODEL_SOURCE))
      .WillOnce(Return(&provider));
  EXPECT_CALL(model_executor_, ExecuteModel(IsForTarget(kTestSegmentId)))
      .Times(1);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true));
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If we just got a new result, we SHOULD NOT try to execute the model.
  auto* prediction_result = segment_info->mutable_prediction_result();
  prediction_result->add_result(0.9);
  prediction_result->set_timestamp_us(
      clock_.Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_CALL(model_executor_, ExecuteModel(_)).Times(0);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));  // Ensure this part has positive result.
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If we have a non-fresh, but not expired result, we SHOULD NOT try to
  // execute the model.
  base::Time not_expired_timestamp =
      clock_.Now() - base::Days(1) + base::Hours(1);
  prediction_result->add_result(0.9);
  prediction_result->set_timestamp_us(
      not_expired_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_CALL(model_executor_, ExecuteModel(_)).Times(0);
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If we have an expired result, we SHOULD try to execute the model.
  base::Time just_expired_timestamp =
      clock_.Now() - base::Days(1) - base::Hours(1);
  prediction_result->add_result(0.9);
  prediction_result->set_timestamp_us(
      just_expired_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_CALL(
      model_manager_,
      GetModelProvider(kTestSegmentId, proto::ModelSource::SERVER_MODEL_SOURCE))
      .WillOnce(Return(&provider));
  EXPECT_CALL(model_executor_, ExecuteModel(IsForTarget(kTestSegmentId)))
      .Times(1);
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);
}

TEST_F(ModelExecutionSchedulerTest, RequestModelExecutionForEligibleSegments) {
  MockModelProvider provider(kTestSegmentId, base::DoNothing());
  segment_database_->FindOrCreateSegment(kTestSegmentId);
  segment_database_->FindOrCreateSegment(kTestOptimizationTarget2);

  // TODO(shaktisahu): Add tests for expired segments, freshly computed segments
  // etc.

  EXPECT_CALL(
      model_manager_,
      GetModelProvider(kTestSegmentId, proto::ModelSource::SERVER_MODEL_SOURCE))
      .WillOnce(Return(&provider));
  EXPECT_CALL(model_executor_, ExecuteModel(IsForTarget(kTestSegmentId)))
      .Times(1);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(model_executor_,
              ExecuteModel(IsForTarget(kTestOptimizationTarget2)))
      .Times(0);
  // TODO(shaktisahu): Add test when the signal collection returns false.

  model_execution_scheduler_->RequestModelExecutionForEligibleSegments(true);
}

TEST_F(ModelExecutionSchedulerTest, OnModelExecutionCompleted) {
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(kTestSegmentId);

  // TODO(shaktisahu): Add tests for model failure.
  EXPECT_CALL(observer2_, OnModelExecutionCompleted(kTestSegmentId)).Times(1);
  EXPECT_CALL(observer1_, OnModelExecutionCompleted(kTestSegmentId)).Times(1);
  float score = 0.4;
  model_execution_scheduler_->OnModelExecutionCompleted(
      *segment_info,
      std::make_unique<ModelExecutionResult>(
          ModelProvider::Request(), ModelProvider::Response(1, score)));

  // Verify that the results are written to the DB.
  segment_info = segment_database_->FindOrCreateSegment(kTestSegmentId);
  ASSERT_TRUE(segment_info->has_prediction_result());
  EXPECT_THAT(segment_info->prediction_result().result(),
              testing::ElementsAre(score));
}

}  // namespace segmentation_platform
