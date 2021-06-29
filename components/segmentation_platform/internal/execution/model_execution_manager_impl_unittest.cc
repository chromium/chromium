// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using testing::_;
using testing::Return;
using testing::SetArgReferee;

namespace segmentation_platform {
using Samples = std::vector<SignalDatabase::Sample>;

class MockSegmentationModelHandler : public SegmentationModelHandler {
 public:
  MockSegmentationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      optimization_guide::proto::OptimizationTarget optimization_target)
      : SegmentationModelHandler(model_provider,
                                 background_task_runner,
                                 optimization_target) {}

  MOCK_METHOD(void,
              ExecuteModelWithInput,
              (base::OnceCallback<void(const absl::optional<float>&)> callback,
               const std::vector<float>& input),
              (override));

  MOCK_METHOD(bool, ModelAvailable, (), (const override));
};

class MockFeatureAggregator : public FeatureAggregator {
 public:
  MockFeatureAggregator() = default;
  MOCK_METHOD(std::vector<float>,
              Process,
              (proto::SignalType signal_type,
               proto::Aggregation aggregation,
               uint64_t length,
               const base::Time& end_time,
               const base::TimeDelta& bucket_duration,
               const Samples& samples),
              (const override));
  MOCK_METHOD(void,
              FilterEnumSamples,
              (const std::vector<int32_t>& accepted_enum_ids, Samples& samples),
              (const override));
};

class ModelExecutionManagerTest : public testing::Test {
 public:
  ModelExecutionManagerTest() = default;
  ~ModelExecutionManagerTest() override = default;

  void SetUp() override {
    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    signal_database_ = std::make_unique<MockSignalDatabase>();
    clock_.SetNow(base::Time::Now());
  }

  void TearDown() override {
    model_execution_manager_.reset();
    // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
    // to be destroyed.
    RunUntilIdle();
  }

  void CreateModelExecutionManager(
      std::vector<OptimizationTarget> segment_ids) {
    auto feature_aggregator = std::make_unique<MockFeatureAggregator>();
    feature_aggregator_ = feature_aggregator.get();

    model_execution_manager_ = std::make_unique<ModelExecutionManagerImpl>(
        segment_ids,
        base::BindRepeating(&ModelExecutionManagerTest::CreateModelHandler,
                            base::Unretained(this)),
        &clock_, segment_database_.get(), signal_database_.get(),
        std::move(feature_aggregator));
  }

  std::unique_ptr<SegmentationModelHandler> CreateModelHandler(
      optimization_guide::proto::OptimizationTarget segment_id) {
    auto handler = std::make_unique<MockSegmentationModelHandler>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(), segment_id);
    model_handlers_.emplace(std::make_pair(segment_id, handler.get()));
    return handler;
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

  MockSegmentationModelHandler& FindHandler(
      optimization_guide::proto::OptimizationTarget segment_id) {
    return *(*model_handlers_.find(segment_id)).second;
  }

  base::Time StartTime(base::TimeDelta bucket_duration, int64_t length) {
    return clock_.Now() - length * bucket_duration;
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::map<OptimizationTarget, MockSegmentationModelHandler*> model_handlers_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<MockSignalDatabase> signal_database_;
  MockFeatureAggregator* feature_aggregator_;

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
  auto unrelated_segment_id =
      optimization_guide::proto::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  CreateModelExecutionManager({segment_id, unrelated_segment_id});

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up a single user action feature.
  std::string user_action_name_1 = "some_action_1";
  segment_database_->AddUserActionFeature(segment_id, user_action_name_1, 2,
                                          proto::Aggregation::SUM_COUNT);

  // When the particular user action is looked up with the correct start time,
  // end time, and aggregation type, return 3 samples.
  Samples samples{
      {clock_.Now(), absl::nullopt},
      {clock_.Now(), absl::nullopt},
      {clock_.Now(), absl::nullopt},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name_1),
                         StartTime(bucket_duration, 2), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(samples));

  // After retrieving the samples, they should be processed and aggregated.
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::USER_ACTION, proto::Aggregation::SUM_COUNT, 2,
              clock_.Now(), bucket_duration, samples))
      .WillOnce(Return(std::vector<float>{3}));

  // The next step should be to execute the model.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{3}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::SUCCESS));
}

TEST_F(ModelExecutionManagerTest, ModelNotReady) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id});

  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);

  // When the model is unavailable, the execution should fail.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(false));

  ExecuteModel(std::make_pair(0, ModelExecutionStatus::EXECUTION_ERROR));
}

TEST_F(ModelExecutionManagerTest, MultipleFeatures) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id});

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up 3 metadata feature, one of each signal type.
  std::string user_action_name = "some_user_action";
  segment_database_->AddUserActionFeature(segment_id, user_action_name, 2,
                                          proto::Aggregation::SUM_COUNT);
  std::string histogram_value_name = "some_histogram_value";
  segment_database_->AddHistogramValueFeature(
      segment_id, histogram_value_name, 3, proto::Aggregation::SUM_VALUES);
  std::string histogram_enum_name = "some_histogram_enum";
  segment_database_->AddHistogramEnumFeature(segment_id, histogram_enum_name, 4,
                                             proto::Aggregation::SUM_COUNT, {});

  // First feature should be the user action.
  Samples user_action_samples{
      {clock_.Now(), absl::nullopt},
      {clock_.Now(), absl::nullopt},
      {clock_.Now(), absl::nullopt},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name),
                         StartTime(bucket_duration, 2), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(user_action_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::USER_ACTION, proto::Aggregation::SUM_COUNT, 2,
              clock_.Now(), bucket_duration, user_action_samples))
      .WillOnce(Return(std::vector<float>{3}));

  // Second feature should be the value histogram.
  Samples histogram_value_samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_VALUE,
                         base::HashMetricName(histogram_value_name),
                         StartTime(bucket_duration, 3), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_value_samples));
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::HISTOGRAM_VALUE,
                      proto::Aggregation::SUM_VALUES, 3, clock_.Now(),
                      bucket_duration, histogram_value_samples))
      .WillOnce(Return(std::vector<float>{6}));

  // Third feature should be the value histogram.
  Samples histogram_enum_samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
      {clock_.Now(), 4},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                         base::HashMetricName(histogram_enum_name),
                         StartTime(bucket_duration, 4), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_enum_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_ENUM, proto::Aggregation::SUM_COUNT,
              4, clock_.Now(), bucket_duration, histogram_enum_samples))
      .WillOnce(Return(std::vector<float>{4}));

  // The input tensor should contain all three values: 3, 6, and 4.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{3, 6, 4}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::SUCCESS));
}

TEST_F(ModelExecutionManagerTest, FilteredEnumSamples) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id});

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up a single enum histogram feature.
  std::string histogram_enum_name = "some_histogram_enum";
  std::vector<int32_t> accepted_enum_ids = {2, 4};
  segment_database_->AddHistogramEnumFeature(segment_id, histogram_enum_name, 4,
                                             proto::Aggregation::SUM_COUNT,
                                             accepted_enum_ids);

  // When the particular enum histogram is looked up with the correct start
  // time, end time, and aggregation type, return all 5 samples.
  Samples histogram_enum_samples{
      {clock_.Now(), 1}, {clock_.Now(), 2}, {clock_.Now(), 3},
      {clock_.Now(), 4}, {clock_.Now(), 5},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                         base::HashMetricName(histogram_enum_name),
                         StartTime(bucket_duration, 4), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_enum_samples));
  // The executor must first filter the enum samples.
  Samples filtered_enum_samples{
      {clock_.Now(), 2},
      {clock_.Now(), 4},
  };
  EXPECT_CALL(*feature_aggregator_,
              FilterEnumSamples(accepted_enum_ids, histogram_enum_samples))
      .WillOnce(SetArgReferee<1>(filtered_enum_samples));
  // Only filtered_enum_samples should be processed.
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_ENUM, proto::Aggregation::SUM_COUNT,
              4, clock_.Now(), bucket_duration, filtered_enum_samples))
      .WillOnce(Return(std::vector<float>{2}));

  // The input tensor should contain a single value.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{2}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::SUCCESS));
}

}  // namespace segmentation_platform
