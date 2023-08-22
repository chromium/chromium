// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_aggregator.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using SignalDatabaseSample = segmentation_platform::SignalDatabase::Sample;
using testing::_;
using testing::Return;
using testing::SetArgReferee;

namespace segmentation_platform::processing {

namespace {
constexpr base::TimeDelta kOneSecond = base::Seconds(1);
constexpr base::TimeDelta kTwoSeconds = base::Seconds(2);
}  // namespace

class FeatureListQueryProcessorTest : public testing::Test {
 public:
  FeatureListQueryProcessorTest() = default;
  ~FeatureListQueryProcessorTest() override = default;
  void SetUp() override {
    auto moved_signal_db = std::make_unique<MockSignalDatabase>();
    signal_database_ = moved_signal_db.get();
    storage_service_ = std::make_unique<StorageService>(
        nullptr, std::move(moved_signal_db), nullptr, nullptr, nullptr,
        &ukm_data_manager_);
    clock_.SetNow(base::Time::Now());
    segment_id_ = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  }

  void TearDown() override {
    feature_list_query_processor_.reset();
    // Allow for the background class to be destroyed.
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void CreateFeatureListQueryProcessor() {
    auto feature_aggregator = std::make_unique<MockFeatureAggregator>();
    feature_aggregator_ = feature_aggregator.get();
    feature_list_query_processor_ = std::make_unique<FeatureListQueryProcessor>(
        storage_service_.get(),
        std::make_unique<processing::InputDelegateHolder>(),
        std::move(feature_aggregator));
  }

  void SetBucketDuration(uint64_t bucket_duration, proto::TimeUnit time_unit) {
    model_metadata.set_bucket_duration(bucket_duration);
    model_metadata.set_time_unit(time_unit);
  }

  base::Time StartTime(base::TimeDelta bucket_duration, int64_t bucket_count) {
    return clock_.Now() - bucket_duration * bucket_count;
  }

  proto::UMAFeature CreateUmaFeature(
      proto::SignalType signal_type,
      const std::string& name,
      uint64_t bucket_count,
      uint64_t tensor_length,
      proto::Aggregation aggregation,
      const std::vector<int32_t>& accepted_enum_ids,
      std::vector<float> default_value) {
    proto::UMAFeature uma_feature;
    uma_feature.set_type(signal_type);
    uma_feature.set_name(name);
    uma_feature.set_name_hash(base::HashMetricName(name));
    uma_feature.set_bucket_count(bucket_count);
    uma_feature.set_tensor_length(tensor_length);
    uma_feature.set_aggregation(aggregation);

    for (float value : default_value) {
      uma_feature.add_default_values(value);
    }
    for (int32_t accepted_enum_id : accepted_enum_ids)
      uma_feature.add_enum_ids(accepted_enum_id);
    return uma_feature;
  }

  void AddUmaFeature(proto::SignalType signal_type,
                     const std::string& name,
                     uint64_t bucket_count,
                     uint64_t tensor_length,
                     proto::Aggregation aggregation,
                     const std::vector<int32_t>& accepted_enum_ids,
                     std::vector<float> default_value = {}) {
    auto* input_feature = model_metadata.add_input_features();
    auto* uma_feature = input_feature->mutable_uma_feature();
    uma_feature->CopyFrom(CreateUmaFeature(signal_type, name, bucket_count,
                                           tensor_length, aggregation,
                                           accepted_enum_ids, default_value));
  }

  void AddOutputUmaFeature(proto::SignalType signal_type,
                           const std::string& name,
                           uint64_t bucket_count,
                           uint64_t tensor_length,
                           proto::Aggregation aggregation,
                           const std::vector<int32_t>& accepted_enum_ids,
                           std::vector<float> default_value = {}) {
    model_metadata.mutable_training_outputs()
        ->add_outputs()
        ->mutable_uma_output()
        ->mutable_uma_feature()
        ->CopyFrom(CreateUmaFeature(signal_type, name, bucket_count,
                                    tensor_length, aggregation,
                                    accepted_enum_ids, default_value));
  }
  void AddUserActionWithProcessingSetup(base::TimeDelta bucket_duration) {
    // Set up a single user action feature.
    std::string user_action_name = "some_action";
    AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name, 2, 1,
                  proto::Aggregation::COUNT, {});

    // When the particular user action is looked up with the correct start time,
    // end time, and aggregation type, return 3 samples.
    std::vector<SignalDatabaseSample> samples{
        {clock_.Now(), 0},
        {clock_.Now(), 0},
        {clock_.Now(), 0},
    };

    EXPECT_CALL(*signal_database_,
                GetSamples(proto::SignalType::USER_ACTION,
                           base::HashMetricName(user_action_name),
                           StartTime(bucket_duration, 2), clock_.Now(), _))
        .WillOnce(RunOnceCallback<4>(samples));

    // After retrieving the samples, they should be processed and aggregated.
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::USER_ACTION, proto::Aggregation::COUNT, 2,
                clock_.Now(), bucket_duration, samples))
        .WillOnce(Return(std::vector<float>{3}));
  }

  void AddCustomInput(int tensor_length,
                      proto::CustomInput::FillPolicy fill_policy,
                      const std::vector<float>& default_values) {
    auto* input_feature = model_metadata.add_input_features();
    proto::CustomInput* custom_input = input_feature->mutable_custom_input();
    custom_input->set_fill_policy(fill_policy);
    custom_input->set_tensor_length(tensor_length);
    for (float default_value : default_values)
      custom_input->add_default_value(default_value);
  }

  void ExpectProcessedFeatureList(
      bool expected_error,
      const ModelProvider::Request& expected_input_tensor,
      const ModelProvider::Response& expected_output_tensor,
      base::Time prediction_time,
      base::Time observation_time = base::Time(),
      FeatureListQueryProcessor::ProcessOption process_option =
          FeatureListQueryProcessor::ProcessOption::kInputsOnly) {
    base::RunLoop loop;
    feature_list_query_processor_->ProcessFeatureList(
        model_metadata, /*input_context=*/nullptr, segment_id_, prediction_time,
        observation_time, process_option,
        base::BindOnce(
            &FeatureListQueryProcessorTest::OnProcessingFinishedCallback,
            base::Unretained(this), loop.QuitClosure(), expected_error,
            expected_input_tensor, expected_output_tensor));
    loop.Run();
  }

  void ExpectProcessedFeatureList(
      bool expected_error,
      const ModelProvider::Request& expected_input_tensor) {
    ExpectProcessedFeatureList(expected_error, expected_input_tensor,
                               ModelProvider::Response(), clock_.Now());
  }

  void OnProcessingFinishedCallback(
      base::RepeatingClosure closure,
      bool expected_error,
      const ModelProvider::Request& expected_input_tensor,
      const ModelProvider::Response& expected_output_tensor,
      bool error,
      const ModelProvider::Request& input_tensor,
      const ModelProvider::Response& output_tensor) {
    EXPECT_EQ(expected_error, error);
    EXPECT_EQ(expected_input_tensor, input_tensor);
    EXPECT_EQ(expected_output_tensor, output_tensor);
    std::move(closure).Run();
  }

  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  SegmentId segment_id_;
  proto::SegmentationModelMetadata model_metadata;
  MockUkmDataManager ukm_data_manager_;
  std::unique_ptr<StorageService> storage_service_;
  raw_ptr<MockSignalDatabase> signal_database_;
  raw_ptr<MockFeatureAggregator, DanglingUntriaged> feature_aggregator_;

  std::unique_ptr<FeatureListQueryProcessor> feature_list_query_processor_;
};

TEST_F(FeatureListQueryProcessorTest, InvalidMetadata) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);

  // Set up a single invalid feature.
  std::string invalid_uma_feature = "invalid_uma_feature";
  AddUmaFeature(proto::SignalType::UNKNOWN_SIGNAL_TYPE, invalid_uma_feature, 2,
                1, proto::Aggregation::COUNT, {});

  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(true, ModelProvider::Request{});
}

TEST_F(FeatureListQueryProcessorTest, PredictionTimeCustomInput) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);

  // Set up a custom input.
  AddCustomInput(1, proto::CustomInput::FILL_PREDICTION_TIME, {});

  // The next step should be to run the feature processor, the input tensor
  // should not allow non float type value such as TIME values.
  ExpectProcessedFeatureList(true, ModelProvider::Request{});
}

TEST_F(FeatureListQueryProcessorTest, DefaultValueCustomInput) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);

  // Set up a custom input.
  AddCustomInput(2, proto::CustomInput::UNKNOWN_FILL_POLICY, {1, 2});

  // The next step should be to run the feature processor, the input tensor
  // should contain the default values 1 and 2.
  ExpectProcessedFeatureList(false, ModelProvider::Request{1, 2});
}

TEST_F(FeatureListQueryProcessorTest, SingleUserAction) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up a single user action feature.
  std::string user_action_name_1 = "some_action_1";
  AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name_1, 2, 1,
                proto::Aggregation::COUNT, {});

  // When the particular user action is looked up with the correct start time,
  // end time, and aggregation type, return 3 samples.
  std::vector<SignalDatabaseSample> samples{
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

  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3});
}

TEST_F(FeatureListQueryProcessorTest, LatestOrDefaultUmaFeature) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up uma features.
  std::string user_action_name_1 = "some_action_1";
  AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name_1, 2, 1,
                proto::Aggregation::LATEST_OR_DEFAULT, {}, {6});

  std::string user_action_name_2 = "some_action_2";
  AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name_2, 2, 1,
                proto::Aggregation::LATEST_OR_DEFAULT, {}, {6});

  // When the particular user action is looked up with the correct start time,
  // end time, and aggregation type, return once with 3 samples and once with
  // empty samples.
  std::vector<SignalDatabaseSample> samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };
  std::vector<SignalDatabaseSample> empty_samples{};

  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name_1),
                         StartTime(bucket_duration, 2), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(samples));

  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name_2),
                         StartTime(bucket_duration, 2), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(empty_samples));

  // After retrieving the samples, they should be processed and aggregated.
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION,
                      proto::Aggregation::LATEST_OR_DEFAULT, 2, clock_.Now(),
                      bucket_duration, samples))
      .WillOnce(Return(std::vector<float>{3}));

  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION,
                      proto::Aggregation::LATEST_OR_DEFAULT, 2, clock_.Now(),
                      bucket_duration, empty_samples))
      .WillOnce(Return(absl::nullopt));

  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 6});
}

TEST_F(FeatureListQueryProcessorTest, UmaFeaturesAndCustomInputs) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up a user action feature.
  AddUserActionWithProcessingSetup(bucket_duration);

  // Set up a custom input.
  AddCustomInput(2, proto::CustomInput::UNKNOWN_FILL_POLICY, {1, 2});

  // The next step should be to run the feature processor, the input tensor
  // should contain {3, 1, 2}.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 1, 2});
}

TEST_F(FeatureListQueryProcessorTest, UmaFeaturesAndCustomInputsInvalid) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up a user action feature.
  AddUserActionWithProcessingSetup(bucket_duration);

  // Set up an invalid custom input.
  AddCustomInput(1, proto::CustomInput::UNKNOWN_FILL_POLICY, {});

  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(true, ModelProvider::Request{});
}

TEST_F(FeatureListQueryProcessorTest, MultipleUmaFeaturesWithOutputs) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up 3 metadata feature, one of each signal type.
  std::string user_action_name = "some_user_action";
  AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name, 2, 1,
                proto::Aggregation::COUNT, {});
  std::string histogram_value_name = "some_histogram_value";
  AddUmaFeature(proto::SignalType::HISTOGRAM_VALUE, histogram_value_name, 3, 1,
                proto::Aggregation::SUM, {});
  std::string histogram_enum_name = "some_histogram_enum";
  AddUmaFeature(proto::SignalType::HISTOGRAM_ENUM, histogram_enum_name, 4, 1,
                proto::Aggregation::COUNT, {});

  // Set up output feature.
  std::string output_histogram_enum_name = "output_histogram_enum";
  AddOutputUmaFeature(proto::SignalType::HISTOGRAM_ENUM,
                      output_histogram_enum_name, 5, 1,
                      proto::Aggregation::COUNT, {});

  // First uma feature should be the user action.
  std::vector<SignalDatabaseSample> user_action_samples{
      {clock_.Now(), 0},
      {clock_.Now(), 0},
      {clock_.Now(), 0},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(user_action_name),
                         StartTime(bucket_duration, 2), clock_.Now(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<4>(user_action_samples));
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION, proto::Aggregation::COUNT,
                      2, clock_.Now(), bucket_duration, user_action_samples))
      .Times(2)
      .WillRepeatedly(Return(std::vector<float>{3}));

  // Second uma feature should be the value histogram.
  std::vector<SignalDatabaseSample> histogram_value_samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_VALUE,
                         base::HashMetricName(histogram_value_name),
                         StartTime(bucket_duration, 3), clock_.Now(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<4>(histogram_value_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_VALUE, proto::Aggregation::SUM, 3,
              clock_.Now(), bucket_duration, histogram_value_samples))
      .Times(2)
      .WillRepeatedly(Return(std::vector<float>{6}));

  // Third uma feature should be the enum histogram.
  std::vector<SignalDatabaseSample> histogram_enum_samples{
      {clock_.Now(), 1},
      {clock_.Now(), 2},
      {clock_.Now(), 3},
      {clock_.Now(), 4},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                         base::HashMetricName(histogram_enum_name),
                         StartTime(bucket_duration, 4), clock_.Now(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<4>(histogram_enum_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_ENUM, proto::Aggregation::COUNT, 4,
              clock_.Now(), bucket_duration, histogram_enum_samples))
      .Times(2)
      .WillRepeatedly(Return(std::vector<float>{4}));

  // The input tensor should contain all three values: 3, 6, and 4.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 6, 4});

  // Output is also enum histogram
  std::vector<SignalDatabaseSample> output_histogram_enum_samples{
      {clock_.Now(), 1}, {clock_.Now(), 2}, {clock_.Now(), 3},
      {clock_.Now(), 4}, {clock_.Now(), 5},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                         base::HashMetricName(output_histogram_enum_name),
                         StartTime(bucket_duration, 5), clock_.Now(), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<4>(output_histogram_enum_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::HISTOGRAM_ENUM, proto::Aggregation::COUNT, 5,
              clock_.Now(), bucket_duration, output_histogram_enum_samples))
      .Times(2)
      .WillRepeatedly(Return(std::vector<float>{5}));
  // The input tensor should contain all three values: {3, 6, 4}, output
  // contains {5}
  ExpectProcessedFeatureList(
      false, ModelProvider::Request{3, 6, 4}, ModelProvider::Response{5},
      clock_.Now(), base::Time(),
      FeatureListQueryProcessor::ProcessOption::kInputsAndOutputs);

  // Only return tensors for output features.
  ExpectProcessedFeatureList(
      false, ModelProvider::Request(), ModelProvider::Response{5}, clock_.Now(),
      base::Time(), FeatureListQueryProcessor::ProcessOption::kOutputsOnly);
}

TEST_F(FeatureListQueryProcessorTest, SkipCollectionOnlyUmaFeatures) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up 3 metadata feature, one of each signal type.
  std::string collected_user_action = "some_user_action";
  AddUmaFeature(proto::SignalType::USER_ACTION, collected_user_action, 1, 1,
                proto::Aggregation::COUNT, {});
  std::string no_collection_user_action = "no_collection_user_action";
  AddUmaFeature(proto::SignalType::USER_ACTION, no_collection_user_action, 0, 0,
                proto::Aggregation::SUM, {});
  std::string no_collection_histogram_value = "no_collection_histogram_value";
  AddUmaFeature(proto::SignalType::HISTOGRAM_VALUE,
                no_collection_histogram_value, 0, 0, proto::Aggregation::SUM,
                {});
  std::string no_collection_histogram_enum = "no_collection_histogram_enum";
  AddUmaFeature(proto::SignalType::HISTOGRAM_ENUM, no_collection_histogram_enum,
                0, 0, proto::Aggregation::SUM, {});
  std::string collected_histogram_value = "collected_histogram_value";
  AddUmaFeature(proto::SignalType::HISTOGRAM_VALUE, collected_histogram_value,
                1, 1, proto::Aggregation::SUM, {});

  // The first uma feature in use should be the very first uma feature.
  std::vector<SignalDatabaseSample> user_action_samples{
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

  // The three uma features in the middle should all be ignored, so the next one
  // should be the last uma feature.
  std::vector<SignalDatabaseSample> histogram_value_samples{
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

  // The input tensor should contain only the first and last uma feature.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 6});
}

TEST_F(FeatureListQueryProcessorTest, SkipNoColumnWeightCustomInput) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);

  // Set up a custom input.
  AddCustomInput(1, proto::CustomInput::UNKNOWN_FILL_POLICY, {1});

  // Set up a few custom input with tensor length of 0.
  AddCustomInput(0, proto::CustomInput::UNKNOWN_FILL_POLICY, {2});
  AddCustomInput(0, proto::CustomInput::UNKNOWN_FILL_POLICY, {3});

  // Set up another custom input with tensor length.
  AddCustomInput(1, proto::CustomInput::UNKNOWN_FILL_POLICY, {4});

  // The next step should be to run the feature processor, the input tensor
  // should contain the first and last custom input of 1 and 4.
  ExpectProcessedFeatureList(false, ModelProvider::Request{1, 4});
}

TEST_F(FeatureListQueryProcessorTest, FilteredEnumSamples) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up a single enum histogram feature.
  std::string histogram_enum_name = "some_histogram_enum";
  std::vector<int32_t> accepted_enum_ids = {2, 4};
  AddUmaFeature(proto::SignalType::HISTOGRAM_ENUM, histogram_enum_name, 4, 1,
                proto::Aggregation::COUNT, accepted_enum_ids);

  // When the particular enum histogram is looked up with the correct start
  // time, end time, and aggregation type, return all 5 samples.
  std::vector<SignalDatabaseSample> histogram_enum_samples{
      {clock_.Now(), 1}, {clock_.Now(), 2}, {clock_.Now(), 3},
      {clock_.Now(), 4}, {clock_.Now(), 5},
  };
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                         base::HashMetricName(histogram_enum_name),
                         StartTime(bucket_duration, 4), clock_.Now(), _))
      .WillOnce(RunOnceCallback<4>(histogram_enum_samples));
  // The executor must first filter the enum samples.
  std::vector<SignalDatabaseSample> filtered_enum_samples{
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
  ExpectProcessedFeatureList(false, ModelProvider::Request{2});
}

TEST_F(FeatureListQueryProcessorTest, MultipleUmaFeaturesWithMultipleBuckets) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up metadata uma features where bucket_count is not equal to 1.
  std::string user_action_name = "some_user_action";
  // 3 buckets
  AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name, 3, 3,
                proto::Aggregation::BUCKETED_COUNT, {});
  std::string histogram_value_name = "some_histogram_value";
  // 4 buckets
  AddUmaFeature(proto::SignalType::HISTOGRAM_VALUE, histogram_value_name, 4, 4,
                proto::Aggregation::BUCKETED_COUNT_BOOLEAN, {});

  // First uma feature should be the user action. The timestamp is set to three
  // different buckets.
  std::vector<SignalDatabaseSample> user_action_samples{
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

  // Second uma feature should be the value histogram. The timestamp is set to
  // four different buckets.
  std::vector<SignalDatabaseSample> histogram_value_samples{
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
  ExpectProcessedFeatureList(false,
                             ModelProvider::Request{1, 2, 3, 4, 5, 6, 7});
}

TEST_F(FeatureListQueryProcessorTest, SingleUmaOutputWithObservationTime) {
  CreateFeatureListQueryProcessor();

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);
  base::Time prediction_time = clock_.Now() - base::Hours(1);
  base::Time observation_time = clock_.Now();
  base::Time start_time = prediction_time - bucket_duration * 2;

  // Set up an output feature.
  std::string output_user_action_name = "output_user_action";
  AddOutputUmaFeature(proto::SignalType::USER_ACTION, output_user_action_name,
                      2, 1, proto::Aggregation::COUNT, {});

  // First uma feature should be the output user action.
  std::vector<SignalDatabaseSample> user_action_samples{
      {clock_.Now(), 0},
      {clock_.Now(), 0},
      {clock_.Now(), 0},
  };
  // Without observation time.
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(output_user_action_name),
                         start_time, prediction_time, _))
      .Times(1)
      .WillRepeatedly(RunOnceCallback<4>(user_action_samples));
  EXPECT_CALL(*feature_aggregator_,
              Process(proto::SignalType::USER_ACTION, proto::Aggregation::COUNT,
                      2, prediction_time, bucket_duration, user_action_samples))
      .Times(1)
      .WillRepeatedly(Return(std::vector<float>{5}));

  // Without observation time, output contains {5}
  ExpectProcessedFeatureList(
      false, ModelProvider::Request(), ModelProvider::Response{5},
      prediction_time, base::Time(),
      FeatureListQueryProcessor::ProcessOption::kOutputsOnly);

  // With observation time.
  EXPECT_CALL(*signal_database_,
              GetSamples(proto::SignalType::USER_ACTION,
                         base::HashMetricName(output_user_action_name),
                         prediction_time, observation_time, _))
      .Times(1)
      .WillRepeatedly(RunOnceCallback<4>(user_action_samples));
  EXPECT_CALL(
      *feature_aggregator_,
      Process(proto::SignalType::USER_ACTION, proto::Aggregation::COUNT, 2,
              observation_time, bucket_duration, user_action_samples))
      .Times(1)
      .WillRepeatedly(Return(std::vector<float>{3}));

  // With observation time, output contains {3}
  ExpectProcessedFeatureList(
      false, ModelProvider::Request(), ModelProvider::Response{3},
      prediction_time, observation_time,
      FeatureListQueryProcessor::ProcessOption::kOutputsOnly);
}

}  // namespace segmentation_platform::processing
