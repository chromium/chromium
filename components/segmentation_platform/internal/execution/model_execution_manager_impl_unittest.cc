// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using testing::_;
using testing::Return;
using testing::SaveArg;
using testing::SetArgReferee;

namespace segmentation_platform {
using Sample = SignalDatabase::Sample;

namespace {
constexpr base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kTwoSeconds = base::TimeDelta::FromSeconds(2);
}  // namespace

class MockSegmentInfoDatabase : public test::TestSegmentInfoDatabase {
 public:
  MOCK_METHOD(void, Initialize, (SuccessCallback callback), (override));
  MOCK_METHOD(void,
              GetAllSegmentInfo,
              (MultipleSegmentInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetSegmentInfoForSegments,
              (const std::vector<OptimizationTarget>& segment_ids,
               MultipleSegmentInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetSegmentInfo,
              (OptimizationTarget segment_id, SegmentInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateSegment,
              (OptimizationTarget segment_id,
               absl::optional<proto::SegmentInfo> segment_info,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              SaveSegmentResult,
              (OptimizationTarget segment_id,
               absl::optional<proto::PredictionResult> result,
               SuccessCallback callback),
              (override));
};

class MockSegmentationModelHandler : public SegmentationModelHandler {
 public:
  MockSegmentationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      optimization_guide::proto::OptimizationTarget optimization_target,
      const SegmentationModelHandler::ModelUpdatedCallback&
          model_updated_callback)
      : SegmentationModelHandler(model_provider,
                                 background_task_runner,
                                 optimization_target,
                                 model_updated_callback) {}

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
               uint64_t bucket_count,
               const base::Time& end_time,
               const base::TimeDelta& bucket_duration,
               const std::vector<Sample>& samples),
              (const override));
  MOCK_METHOD(void,
              FilterEnumSamples,
              (const std::vector<int32_t>& accepted_enum_ids,
               std::vector<Sample>& samples),
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
      std::vector<OptimizationTarget> segment_ids,
      const ModelExecutionManager::SegmentationModelUpdatedCallback& callback) {
    auto feature_aggregator = std::make_unique<MockFeatureAggregator>();
    feature_aggregator_ = feature_aggregator.get();

    model_execution_manager_ = std::make_unique<ModelExecutionManagerImpl>(
        segment_ids,
        base::BindRepeating(&ModelExecutionManagerTest::CreateModelHandler,
                            base::Unretained(this)),
        &clock_, segment_database_.get(), signal_database_.get(),
        std::move(feature_aggregator), callback);
  }

  std::unique_ptr<SegmentationModelHandler> CreateModelHandler(
      optimization_guide::proto::OptimizationTarget segment_id,
      const SegmentationModelHandler::ModelUpdatedCallback&
          model_updated_callback) {
    auto handler = std::make_unique<MockSegmentationModelHandler>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(), segment_id,
        model_updated_callback);
    model_handlers_.emplace(std::make_pair(segment_id, handler.get()));
    model_handlers_callbacks_.emplace(
        std::make_pair(segment_id, model_updated_callback));
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

  base::Time StartTime(base::TimeDelta bucket_duration, int64_t bucket_count) {
    return clock_.Now() - bucket_duration * bucket_count;
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::map<OptimizationTarget, MockSegmentationModelHandler*> model_handlers_;
  std::map<OptimizationTarget, SegmentationModelHandler::ModelUpdatedCallback>
      model_handlers_callbacks_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<MockSignalDatabase> signal_database_;
  MockFeatureAggregator* feature_aggregator_;

  std::unique_ptr<ModelExecutionManagerImpl> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, HandlerNotRegistered) {
  CreateModelExecutionManager({}, base::DoNothing());
  EXPECT_DCHECK_DEATH(
      ExecuteModel(std::make_pair(0, ModelExecutionStatus::kExecutionError)));
}

TEST_F(ModelExecutionManagerTest, MetadataTests) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, base::DoNothing());
  ExecuteModel(std::make_pair(0, ModelExecutionStatus::kInvalidMetadata));

  segment_database_->SetBucketDuration(segment_id, 14,
                                       proto::TimeUnit::UNKNOWN_TIME_UNIT);
  ExecuteModel(std::make_pair(0, ModelExecutionStatus::kInvalidMetadata));
}

TEST_F(ModelExecutionManagerTest, SingleUserAction) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  auto unrelated_segment_id =
      optimization_guide::proto::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
  CreateModelExecutionManager({segment_id, unrelated_segment_id},
                              base::DoNothing());

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up a single user action feature.
  std::string user_action_name_1 = "some_action_1";
  segment_database_->AddUserActionFeature(segment_id, user_action_name_1, 2, 1,
                                          proto::Aggregation::COUNT);

  // When the particular user action is looked up with the correct start time,
  // end time, and aggregation type, return 3 samples.
  std::vector<Sample> samples{
      {clock_.Now(), 0},
      {clock_.Now(), 0},
      {clock_.Now(), 0},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name_1),
                         StartTime(bucket_duration, 2), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(samples));

  // After retrieving the samples, they should be processed and aggregated.
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION, proto::Aggregation::COUNT,
                      2, clock_.Now(), bucket_duration, samples))
      .WillOnce(Return(std::vector<float>{3}));

  // The next step should be to execute the model.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{3}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::kSuccess));
}

TEST_F(ModelExecutionManagerTest, ModelNotReady) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, base::DoNothing());

  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);

  // When the model is unavailable, the execution should fail.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(false));

  ExecuteModel(std::make_pair(0, ModelExecutionStatus::kExecutionError));
}

TEST_F(ModelExecutionManagerTest, MultipleFeatures) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, base::DoNothing());

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up 3 metadata feature, one of each signal type.
  std::string user_action_name = "some_user_action";
  segment_database_->AddUserActionFeature(segment_id, user_action_name, 2, 1,
                                          proto::Aggregation::COUNT);
  std::string histogram_value_name = "some_histogram_value";
  segment_database_->AddHistogramValueFeature(segment_id, histogram_value_name,
                                              3, 1, proto::Aggregation::SUM);
  std::string histogram_enum_name = "some_histogram_enum";
  segment_database_->AddHistogramEnumFeature(segment_id, histogram_enum_name, 4,
                                             1, proto::Aggregation::COUNT, {});

  // First feature should be the user action.
  std::vector<Sample> user_action_samples{
      {clock_.Now(), 0},
      {clock_.Now(), 0},
      {clock_.Now(), 0},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name),
                         StartTime(bucket_duration, 2), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(user_action_samples));
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION, proto::Aggregation::COUNT,
                      2, clock_.Now(), bucket_duration, user_action_samples))
      .WillOnce(Return(std::vector<float>{3}));

  // Second feature should be the value histogram.
  std::vector<Sample> histogram_value_samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_VALUE,
                         base::HashMetricName(histogram_value_name),
                         StartTime(bucket_duration, 3), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_value_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_VALUE, proto::Aggregation::SUM, 3,
              clock_.Now(), bucket_duration, histogram_value_samples))
      .WillOnce(Return(std::vector<float>{6}));

  // Third feature should be the value histogram.
  std::vector<Sample> histogram_enum_samples{
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
      Process(proto::SignalType::HISTOGRAM_ENUM, proto::Aggregation::COUNT, 4,
              clock_.Now(), bucket_duration, histogram_enum_samples))
      .WillOnce(Return(std::vector<float>{4}));

  // The input tensor should contain all three values: 3, 6, and 4.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{3, 6, 4}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::kSuccess));
}

TEST_F(ModelExecutionManagerTest, SkipCollectionOnlyFeatures) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, base::DoNothing());

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up 3 metadata feature, one of each signal type.
  std::string collected_user_action = "some_user_action";
  segment_database_->AddUserActionFeature(segment_id, collected_user_action, 1,
                                          1, proto::Aggregation::COUNT);
  std::string no_collection_user_action = "no_collection_user_action";
  segment_database_->AddUserActionFeature(segment_id, no_collection_user_action,
                                          0, 0, proto::Aggregation::SUM);
  std::string no_collection_histogram_value = "no_collection_histogram_value";
  segment_database_->AddHistogramValueFeature(
      segment_id, no_collection_histogram_value, 0, 0, proto::Aggregation::SUM);
  std::string no_collection_histogram_enum = "no_collection_histogram_enum";
  segment_database_->AddHistogramEnumFeature(segment_id,
                                             no_collection_histogram_enum, 0, 0,
                                             proto::Aggregation::SUM, {});
  std::string collected_histogram_value = "collected_histogram_value";
  segment_database_->AddHistogramValueFeature(
      segment_id, collected_histogram_value, 1, 1, proto::Aggregation::SUM);

  // The first feature in use should be the very first feature.
  std::vector<Sample> user_action_samples{
      {clock_.Now(), 0},
      {clock_.Now(), 0},
      {clock_.Now(), 0},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(collected_user_action),
                         StartTime(bucket_duration, 1), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(user_action_samples));
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION, proto::Aggregation::COUNT,
                      1, clock_.Now(), bucket_duration, user_action_samples))
      .WillOnce(Return(std::vector<float>{3}));

  // The three features in the middle should all be ignored, so the next one
  // should be the last feature.
  std::vector<Sample> histogram_value_samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_VALUE,
                         base::HashMetricName(collected_histogram_value),
                         StartTime(bucket_duration, 1), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_value_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_VALUE, proto::Aggregation::SUM, 1,
              clock_.Now(), bucket_duration, histogram_value_samples))
      .WillOnce(Return(std::vector<float>{6}));

  // The input tensor should contain only the first and last feature.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{3, 6}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::kSuccess));
}

TEST_F(ModelExecutionManagerTest, FilteredEnumSamples) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, base::DoNothing());

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up a single enum histogram feature.
  std::string histogram_enum_name = "some_histogram_enum";
  std::vector<int32_t> accepted_enum_ids = {2, 4};
  segment_database_->AddHistogramEnumFeature(segment_id, histogram_enum_name, 4,
                                             1, proto::Aggregation::COUNT,
                                             accepted_enum_ids);

  // When the particular enum histogram is looked up with the correct start
  // time, end time, and aggregation type, return all 5 samples.
  std::vector<Sample> histogram_enum_samples{
      {clock_.Now(), 1}, {clock_.Now(), 2}, {clock_.Now(), 3},
      {clock_.Now(), 4}, {clock_.Now(), 5},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                         base::HashMetricName(histogram_enum_name),
                         StartTime(bucket_duration, 4), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_enum_samples));
  // The executor must first filter the enum samples.
  std::vector<Sample> filtered_enum_samples{
      {clock_.Now(), 2},
      {clock_.Now(), 4},
  };
  EXPECT_CALL(*feature_aggregator_,
              FilterEnumSamples(accepted_enum_ids, histogram_enum_samples))
      .WillOnce(SetArgReferee<1>(filtered_enum_samples));
  // Only filtered_enum_samples should be processed.
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_ENUM, proto::Aggregation::COUNT, 4,
              clock_.Now(), bucket_duration, filtered_enum_samples))
      .WillOnce(Return(std::vector<float>{2}));

  // The input tensor should contain a single value.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{2}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::kSuccess));
}

TEST_F(ModelExecutionManagerTest, MultipleFeaturesWithMultipleBuckets) {
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, base::DoNothing());

  // Initialize with required metadata.
  segment_database_->SetBucketDuration(segment_id, 3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::TimeDelta::FromHours(3);

  // Set up metadata features where bucket_count is not equal to 1.
  std::string user_action_name = "some_user_action";
  // 3 buckets
  segment_database_->AddUserActionFeature(segment_id, user_action_name, 3, 3,
                                          proto::Aggregation::BUCKETED_COUNT);
  std::string histogram_value_name = "some_histogram_value";
  // 4 buckets
  segment_database_->AddHistogramValueFeature(
      segment_id, histogram_value_name, 4, 4,
      proto::Aggregation::BUCKETED_COUNT_BOOLEAN);

  // First feature should be the user action. The timestamp is set to three
  // different buckets.
  std::vector<Sample> user_action_samples{
      {clock_.Now(), 0},
      {clock_.Now() - kOneSecond, 0},
      {clock_.Now() - bucket_duration, 0},
      {clock_.Now() - bucket_duration - kOneSecond, 0},
      {clock_.Now() - bucket_duration - kTwoSeconds, 0},
      {clock_.Now() - bucket_duration * 2, 0},
      {clock_.Now() - bucket_duration * 2 - kOneSecond, 0},
      {clock_.Now() - bucket_duration * 2 - kTwoSeconds, 0},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name),
                         StartTime(bucket_duration, 3), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(user_action_samples));
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION,
                      proto::Aggregation::BUCKETED_COUNT, 3, clock_.Now(),
                      bucket_duration, user_action_samples))
      .WillOnce(Return(std::vector<float>{1, 2, 3}));

  // Second feature should be the value histogram. The timestamp is set to four
  // different buckets.
  std::vector<Sample> histogram_value_samples{
      {clock_.Now(), 1},
      {clock_.Now() - kOneSecond, 2},
      {clock_.Now() - bucket_duration, 3},
      {clock_.Now() - bucket_duration - kOneSecond, 4},
      {clock_.Now() - bucket_duration - kTwoSeconds, 5},
      {clock_.Now() - bucket_duration * 2, 6},
      {clock_.Now() - bucket_duration * 2 - kOneSecond, 7},
      {clock_.Now() - bucket_duration * 2 - kTwoSeconds, 8},
      {clock_.Now() - bucket_duration * 3, 9},
      {clock_.Now() - bucket_duration * 3 - kOneSecond, 10},
      {clock_.Now() - bucket_duration * 3 - kTwoSeconds, 11},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_VALUE,
                         base::HashMetricName(histogram_value_name),
                         StartTime(bucket_duration, 4), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_value_samples));
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::HISTOGRAM_VALUE,
                      proto::Aggregation::BUCKETED_COUNT_BOOLEAN, 4,
                      clock_.Now(), bucket_duration, histogram_value_samples))
      .WillOnce(Return(std::vector<float>{4, 5, 6, 7}));

  // The input tensor should contain all values flattened to a single vector.
  EXPECT_CALL(FindHandler(segment_id), ModelAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(FindHandler(segment_id),
              ExecuteModelWithInput(_, std::vector<float>{1, 2, 3, 4, 5, 6, 7}))
      .WillOnce(RunOnceCallback<0>(absl::make_optional(0.8)));

  ExecuteModel(std::make_pair(0.8, ModelExecutionStatus::kSuccess));
}

TEST_F(ModelExecutionManagerTest, OnSegmentationModelUpdatedInvalidMetadata) {
  // Use a MockSegmentInfoDatabase for this test in particular, to verify that
  // it is never used.
  auto mock_segment_database = std::make_unique<MockSegmentInfoDatabase>();
  auto* mock_segment_database_ptr = mock_segment_database.get();
  segment_database_ = std::move(mock_segment_database);

  // Construct the ModelExecutionManager.
  base::MockCallback<ModelExecutionManager::SegmentationModelUpdatedCallback>
      callback;
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, callback.Get());

  // Create invalid metadata, which should be ignored.
  proto::SegmentInfo segment_info;
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);

  // Verify that the ModelExecutionManager never invokes its
  // SegmentInfoDatabase, nor invokes the callback.
  EXPECT_CALL(*mock_segment_database_ptr, GetSegmentInfo(_, _)).Times(0);
  EXPECT_CALL(callback, Run(_)).Times(0);
  model_handlers_callbacks_[segment_id].Run(segment_id, metadata);
}

TEST_F(ModelExecutionManagerTest, OnSegmentationModelUpdatedNoOldMetadata) {
  base::MockCallback<ModelExecutionManager::SegmentationModelUpdatedCallback>
      callback;
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, callback.Get());

  proto::SegmentInfo segment_info;
  proto::SegmentationModelMetadata metadata;
  metadata.set_bucket_duration(42u);
  metadata.set_time_unit(proto::TimeUnit::DAY);
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&segment_info));
  model_handlers_callbacks_[segment_id].Run(segment_id, metadata);

  // Verify that the resulting callback was invoked correctly.
  EXPECT_EQ(segment_id, segment_info.segment_id());
  EXPECT_EQ(42u, segment_info.model_metadata().bucket_duration());

  // Also verify that the database has been updated.
  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback;
  absl::optional<proto::SegmentInfo> segment_info_from_db;
  EXPECT_CALL(db_callback, Run(_)).WillOnce(SaveArg<0>(&segment_info_from_db));

  // Fetch SegmentInfo from the database.
  segment_database_->GetSegmentInfo(segment_id, db_callback.Get());
  EXPECT_TRUE(segment_info_from_db.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db->segment_id());

  // The metadata should have been stored.
  EXPECT_EQ(42u, segment_info_from_db->model_metadata().bucket_duration());
}

TEST_F(ModelExecutionManagerTest,
       OnSegmentationModelUpdatedWithPreviousMetadataAndPredictionResult) {
  base::MockCallback<ModelExecutionManager::SegmentationModelUpdatedCallback>
      callback;
  auto segment_id =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  CreateModelExecutionManager({segment_id}, callback.Get());

  // Fill in old data in the SegmentInfo database.
  segment_database_->SetBucketDuration(segment_id, 456, proto::TimeUnit::MONTH);
  segment_database_->AddUserActionFeature(segment_id, "hello", 2, 2,
                                          proto::Aggregation::BUCKETED_COUNT);
  segment_database_->AddPredictionResult(segment_id, 2, clock_.Now());

  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback_1;
  absl::optional<proto::SegmentInfo> segment_info_from_db_1;
  EXPECT_CALL(db_callback_1, Run(_))
      .WillOnce(SaveArg<0>(&segment_info_from_db_1));
  segment_database_->GetSegmentInfo(segment_id, db_callback_1.Get());
  EXPECT_TRUE(segment_info_from_db_1.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db_1->segment_id());
  // Verify the old metadata and prediction result has been stored correctly.
  EXPECT_EQ(456u, segment_info_from_db_1->model_metadata().bucket_duration());
  EXPECT_EQ(2, segment_info_from_db_1->prediction_result().result());
  // Verify the metadata features have been stored correctly.
  EXPECT_EQ(proto::SignalType::USER_ACTION,
            segment_info_from_db_1->model_metadata().features(0).type());
  EXPECT_EQ("hello",
            segment_info_from_db_1->model_metadata().features(0).name());
  EXPECT_EQ(proto::Aggregation::BUCKETED_COUNT,
            segment_info_from_db_1->model_metadata().features(0).aggregation());

  // Create segment info that does not match.
  proto::SegmentInfo segment_info;
  proto::SegmentationModelMetadata metadata;
  metadata.set_bucket_duration(42u);
  metadata.set_time_unit(proto::TimeUnit::HOUR);

  // Create one feature that does not match the stored feature.
  auto* feature = metadata.add_features();
  feature->set_type(proto::SignalType::HISTOGRAM_VALUE);
  feature->set_name("other");
  // Intentionally not set the name hash, as it should be set automatically.
  feature->set_aggregation(proto::Aggregation::BUCKETED_SUM);
  feature->set_bucket_count(3);
  feature->set_tensor_length(3);

  // Invoke the callback and store the resulting invocation of the outer
  // callback for verification.
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&segment_info));
  model_handlers_callbacks_[segment_id].Run(segment_id, metadata);

  // Should now have the metadata from the new proto.
  EXPECT_EQ(segment_id, segment_info.segment_id());
  EXPECT_EQ(42u, segment_info.model_metadata().bucket_duration());
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE,
            segment_info.model_metadata().features(0).type());
  EXPECT_EQ("other", segment_info.model_metadata().features(0).name());
  // The name_hash should have been set automatically.
  EXPECT_EQ(base::HashMetricName("other"),
            segment_info.model_metadata().features(0).name_hash());
  EXPECT_EQ(proto::Aggregation::BUCKETED_SUM,
            segment_info.model_metadata().features(0).aggregation());
  EXPECT_EQ(2, segment_info.prediction_result().result());
  EXPECT_EQ(clock_.Now().ToDeltaSinceWindowsEpoch().InMicroseconds(),
            segment_info.prediction_result().timestamp_us());

  // Also verify that the database has been updated.
  base::MockCallback<SegmentInfoDatabase::SegmentInfoCallback> db_callback_2;
  absl::optional<proto::SegmentInfo> segment_info_from_db_2;
  EXPECT_CALL(db_callback_2, Run(_))
      .WillOnce(SaveArg<0>(&segment_info_from_db_2));
  segment_database_->GetSegmentInfo(segment_id, db_callback_2.Get());
  EXPECT_TRUE(segment_info_from_db_2.has_value());
  EXPECT_EQ(segment_id, segment_info_from_db_2->segment_id());

  // The metadata should have been updated.
  EXPECT_EQ(42u, segment_info_from_db_2->model_metadata().bucket_duration());
  // The metadata features should have been updated.
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE,
            segment_info_from_db_2->model_metadata().features(0).type());
  EXPECT_EQ("other",
            segment_info_from_db_2->model_metadata().features(0).name());
  EXPECT_EQ(base::HashMetricName("other"),
            segment_info_from_db_2->model_metadata().features(0).name_hash());
  EXPECT_EQ(proto::Aggregation::BUCKETED_SUM,
            segment_info_from_db_2->model_metadata().features(0).aggregation());
  // We shuold have kept the prediction result.
  EXPECT_EQ(2, segment_info_from_db_2->prediction_result().result());
}

}  // namespace segmentation_platform
