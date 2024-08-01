// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database/ukm_database_backend.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_aggregator.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallbackRepeatedly;
using testing::_;
using testing::IsEmpty;
using testing::Return;
using testing::SetArgReferee;

namespace segmentation_platform::processing {

namespace {
constexpr base::TimeDelta kOneSecond = base::Seconds(1);
constexpr base::TimeDelta kTwoSeconds = base::Seconds(2);
constexpr char kProfileId[] = "profile_id";
}  // namespace

class FeatureListQueryProcessorTest : public testing::TestWithParam<bool> {
 public:
  FeatureListQueryProcessorTest() = default;
  ~FeatureListQueryProcessorTest() override = default;

  void SetUp() override {
    InitFeatureList();

    ukm_db_ = std::make_unique<UkmDatabaseBackend>(
        base::FilePath(), /*in_memory=*/true,
        task_environment_.GetMainThreadTaskRunner());
    base::RunLoop wait_for_sql_init;
    ukm_db_->InitDatabase(base::BindOnce(
        [](base::OnceClosure quit_closure, bool success) {
          ASSERT_TRUE(success);
          std::move(quit_closure).Run();
        },
        wait_for_sql_init.QuitClosure()));
    wait_for_sql_init.Run();
    mock_ukm_data_manager_ = std::make_unique<MockUkmDataManager>();
    EXPECT_CALL(*mock_ukm_data_manager_, HasUkmDatabase())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_ukm_data_manager_, GetUkmDatabase())
        .WillRepeatedly(testing::Return(ukm_db_.get()));

    auto moved_signal_db = std::make_unique<MockSignalDatabase>();
    signal_database_ = moved_signal_db.get();
    storage_service_ = std::make_unique<StorageService>(
        nullptr, std::move(moved_signal_db), nullptr, nullptr, nullptr,
        mock_ukm_data_manager_.get());
    storage_service_->set_profile_id_for_testing(kProfileId);
    clock_.SetNow(base::Time::Now());
    segment_id_ = SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  }

  void TearDown() override {
    feature_list_query_processor_.reset();
    // Allow for the background class to be destroyed.
    RunUntilIdle();
  }

  virtual void InitFeatureList() {
    feature_list_.InitAndEnableFeature(
        features::kSegmentationPlatformSignalDbCache);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void CreateFeatureListQueryProcessor(bool use_sql_db) {
    if (use_sql_db) {
      uma_from_sql_feature_list_.InitAndEnableFeature(
          features::kSegmentationPlatformUmaFromSqlDb);
    } else {
      uma_from_sql_feature_list_.InitAndDisableFeature(
          features::kSegmentationPlatformUmaFromSqlDb);
    }
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

  void AddUserActionWithProcessingSetup(base::TimeDelta bucket_duration,
                                        bool use_sql_db) {
    // Set up a single user action feature.
    std::string user_action_name = "some_action";
    AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name, 2, 1,
                  proto::Aggregation::COUNT, {});

    // When the particular user action is looked up with the correct start time,
    // end time, and aggregation type, return 3 samples.
    std::vector<SignalDatabase::DbEntry> samples{
        SignalDatabase::DbEntry{
            .type = proto::SignalType::USER_ACTION,
            .name_hash = base::HashMetricName(user_action_name),
            .time = clock_.Now(),
            .value = 0},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::USER_ACTION,
            .name_hash = base::HashMetricName(user_action_name),
            .time = clock_.Now(),
            .value = 0},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::USER_ACTION,
            .name_hash = base::HashMetricName(user_action_name),
            .time = clock_.Now(),
            .value = 0},
    };

    AddSamplesToSqlDb(samples);
    if (!use_sql_db) {
      EXPECT_CALL(*signal_database_, GetAllSamples())
          .WillOnce(Return(&samples));

      // After retrieving the samples, they should be processed and aggregated.
      EXPECT_CALL(
          *feature_aggregator_,
          Process(proto::SignalType::USER_ACTION,
                  base::HashMetricName(user_action_name),
                  proto::Aggregation::COUNT, 2, StartTime(bucket_duration, 2),
                  clock_.Now(), bucket_duration, IsEmpty(), _))
          .WillOnce(Return(std::vector<float>{3}));
    }
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

  void AddSamplesToSqlDb(const std::vector<SignalDatabase::DbEntry>& samples) {
    for (const auto& sample : samples) {
      UmaMetricEntry entry{.type = sample.type,
                           .name_hash = sample.name_hash,
                           .time = sample.time,
                           .value = sample.value};
      ukm_db_->AddUmaMetric(kProfileId, entry);
    }
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

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList uma_from_sql_feature_list_;
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  SegmentId segment_id_;
  proto::SegmentationModelMetadata model_metadata;
  std::unique_ptr<MockUkmDataManager> mock_ukm_data_manager_;
  std::unique_ptr<UkmDatabase> ukm_db_;
  std::unique_ptr<StorageService> storage_service_;
  raw_ptr<MockSignalDatabase> signal_database_;
  raw_ptr<MockFeatureAggregator, DanglingUntriaged> feature_aggregator_;

  std::unique_ptr<FeatureListQueryProcessor> feature_list_query_processor_;
};

INSTANTIATE_TEST_SUITE_P(SqlDbAvailability,
                         FeatureListQueryProcessorTest,
                         testing::Values(true, false));

TEST_P(FeatureListQueryProcessorTest, InvalidMetadata) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);

  // Set up a single invalid feature.
  std::string invalid_uma_feature = "invalid_uma_feature";
  AddUmaFeature(proto::SignalType::UNKNOWN_SIGNAL_TYPE, invalid_uma_feature, 2,
                1, proto::Aggregation::COUNT, {});

  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(true, ModelProvider::Request{});
}

TEST_P(FeatureListQueryProcessorTest, PredictionTimeCustomInput) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);

  // Set up a custom input.
  AddCustomInput(1, proto::CustomInput::FILL_PREDICTION_TIME, {});

  // The next step should be to run the feature processor, the input tensor
  // should not allow non float type value such as TIME values.
  ExpectProcessedFeatureList(true, ModelProvider::Request{});
}

TEST_P(FeatureListQueryProcessorTest, DefaultValueCustomInput) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);

  // Set up a custom input.
  AddCustomInput(2, proto::CustomInput::UNKNOWN_FILL_POLICY, {1, 2});

  // The next step should be to run the feature processor, the input tensor
  // should contain the default values 1 and 2.
  ExpectProcessedFeatureList(false, ModelProvider::Request{1, 2});
}

TEST_P(FeatureListQueryProcessorTest, SingleUserAction) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up a single user action feature.
  std::string user_action_name_1 = "some_action_1";
  AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name_1, 2, 1,
                proto::Aggregation::COUNT, {});

  // When the particular user action is looked up with the correct start time,
  // end time, and aggregation type, return 3 samples.
  std::vector<SignalDatabase::DbEntry> samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name_1),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name_1),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name_1),
          .time = clock_.Now(),
          .value = 0},
  };
  AddSamplesToSqlDb(samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_, GetAllSamples()).WillOnce(Return(&samples));

    // After retrieving the samples, they should be processed and aggregated.
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::USER_ACTION,
                base::HashMetricName(user_action_name_1),
                proto::Aggregation::COUNT, 2, StartTime(bucket_duration, 2),
                clock_.Now(), bucket_duration, std::vector<int32_t>(), _))
        .WillOnce(Return(std::vector<float>{3}));
  }
  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3});
}

TEST_P(FeatureListQueryProcessorTest, LatestOrDefaultUmaFeature) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

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
  std::vector<SignalDatabase::DbEntry> samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name_1),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name_1),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name_1),
          .time = clock_.Now(),
          .value = 3},
  };

  AddSamplesToSqlDb(samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_, GetAllSamples()).WillOnce(Return(&samples));

    // After retrieving the samples, they should be processed and aggregated.
    EXPECT_CALL(*feature_aggregator_,
                Process(proto::SignalType::USER_ACTION, _,
                        proto::Aggregation::LATEST_OR_DEFAULT, 2,
                        StartTime(bucket_duration, 2), clock_.Now(),
                        bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillOnce(Return(std::vector<float>{3}))
        .WillOnce(Return(std::nullopt));
  }

  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 6});
}

TEST_P(FeatureListQueryProcessorTest, UmaFeaturesAndCustomInputs) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up a user action feature.
  AddUserActionWithProcessingSetup(bucket_duration, use_sql_db);

  // Set up a custom input.
  AddCustomInput(2, proto::CustomInput::UNKNOWN_FILL_POLICY, {1, 2});

  // The next step should be to run the feature processor, the input tensor
  // should contain {3, 1, 2}.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 1, 2});
}

TEST_P(FeatureListQueryProcessorTest, UmaFeaturesAndCustomInputsInvalid) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up a user action feature.
  AddUserActionWithProcessingSetup(bucket_duration, use_sql_db);

  // Set up an invalid custom input.
  AddCustomInput(1, proto::CustomInput::UNKNOWN_FILL_POLICY, {});

  // The next step should be to run the feature processor.
  ExpectProcessedFeatureList(true, ModelProvider::Request{});
}

TEST_P(FeatureListQueryProcessorTest, MultipleUmaFeaturesWithOutputs) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

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
  std::vector<SignalDatabase::DbEntry> user_action_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now(),
          .value = 3},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 3},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 4},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 3},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 4},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 5},
  };
  AddSamplesToSqlDb(user_action_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_, GetAllSamples())
        .WillRepeatedly(Return(&user_action_samples));
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::USER_ACTION,
                base::HashMetricName(user_action_name),
                proto::Aggregation::COUNT, 2, StartTime(bucket_duration, 2),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{3}));

    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_VALUE,
                base::HashMetricName(histogram_value_name),
                proto::Aggregation::SUM, 3, StartTime(bucket_duration, 3),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{6}));

    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_ENUM,
                base::HashMetricName(histogram_enum_name),
                proto::Aggregation::COUNT, 4, StartTime(bucket_duration, 4),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{4}));
  }

  // The input tensor should contain all three values: 3, 6, and 4.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 6, 4});

  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_, GetAllSamples())
        .Times(3)
        .WillRepeatedly(Return(&user_action_samples));

    // Output is also enum histogram
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_ENUM,
                base::HashMetricName(output_histogram_enum_name),
                proto::Aggregation::COUNT, 5, StartTime(bucket_duration, 5),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{5}));
  }
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

TEST_P(FeatureListQueryProcessorTest, SkipCollectionOnlyUmaFeatures) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up 5 metadata feature, one of each signal type.
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
  // The three uma features in the middle should all be ignored, so the next one
  // should be the last uma feature.
  std::vector<SignalDatabase::DbEntry> user_action_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(collected_user_action),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(collected_user_action),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(collected_user_action),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(collected_histogram_value),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(collected_histogram_value),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(collected_histogram_value),
          .time = clock_.Now(),
          .value = 3},
  };
  AddSamplesToSqlDb(user_action_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_, GetAllSamples())
        .WillOnce(Return(&user_action_samples));
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::USER_ACTION,
                base::HashMetricName(collected_user_action),
                proto::Aggregation::COUNT, 1, StartTime(bucket_duration, 1),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .WillOnce(Return(std::vector<float>{3}));

    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_VALUE,
                base::HashMetricName(collected_histogram_value),
                proto::Aggregation::SUM, 1, StartTime(bucket_duration, 1),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .WillOnce(Return(std::vector<float>{6}));
  }
  // The input tensor should contain only the first and last uma feature.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 6});
}

TEST_P(FeatureListQueryProcessorTest, SkipNoColumnWeightCustomInput) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

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

TEST_P(FeatureListQueryProcessorTest, FilteredEnumSamples) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

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
  std::vector<SignalDatabase::DbEntry> histogram_enum_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 3},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 4},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 5},
  };
  AddSamplesToSqlDb(histogram_enum_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_, GetAllSamples())
        .WillOnce(Return(&histogram_enum_samples));
  }
  // The executor must first filter the enum samples.
  std::vector<SignalDatabase::DbEntry> filtered_enum_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 4},
  };
  if (!use_sql_db) {
    // Only filtered_enum_samples should be processed.
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_ENUM,
                base::HashMetricName(histogram_enum_name),
                proto::Aggregation::COUNT, 4, StartTime(bucket_duration, 4),
                clock_.Now(), bucket_duration, accepted_enum_ids, _))
        .WillOnce(Return(std::vector<float>{2}));
  }

  // The input tensor should contain a single value.
  ExpectProcessedFeatureList(false, ModelProvider::Request{2});
}

TEST_P(FeatureListQueryProcessorTest, MultipleUmaFeaturesWithMultipleBuckets) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

  // Initialize with required metadata.
  SetBucketDuration(3, proto::TimeUnit::HOUR);
  base::TimeDelta bucket_duration = base::Hours(3);

  // Set up metadata uma features where bucket_count is not equal to 1.
  std::string user_action_name = "some_user_action";
  // 3 buckets
  AddUmaFeature(proto::SignalType::USER_ACTION, user_action_name, 3, 3,
                proto::Aggregation::BUCKETED_COUNT, {});
  std::string histogram_value_name = "some_histogram_value";
  // 5 buckets
  AddUmaFeature(proto::SignalType::HISTOGRAM_VALUE, histogram_value_name, 5, 5,
                proto::Aggregation::BUCKETED_COUNT_BOOLEAN, {});

  // First uma feature should be the user action. The timestamp is set to three
  // different buckets.
  std::vector<SignalDatabase::DbEntry> user_action_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now() - kOneSecond,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now() - bucket_duration,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now() - bucket_duration - kOneSecond,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now() - bucket_duration - kTwoSeconds,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now() - bucket_duration * 2,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now() - bucket_duration * 2 - kOneSecond,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now() - bucket_duration * 2 - kTwoSeconds,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - kOneSecond,
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration,
          .value = 3},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration - kOneSecond,
          .value = 4},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration - kTwoSeconds,
          .value = 5},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration * 2,
          .value = 6},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration * 2 - kOneSecond,
          .value = 7},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration * 2 - kTwoSeconds,
          .value = 8},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration * 3,
          .value = 9},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration * 3 - kOneSecond,
          .value = 10},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now() - bucket_duration * 3 - kTwoSeconds,
          .value = 11},
  };
  AddSamplesToSqlDb(user_action_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_, GetAllSamples())
        .WillOnce(Return(&user_action_samples));
    EXPECT_CALL(*feature_aggregator_,
                Process(proto::SignalType::USER_ACTION,
                        base::HashMetricName(user_action_name),
                        proto::Aggregation::BUCKETED_COUNT, 3,
                        StartTime(bucket_duration, 3), clock_.Now(),
                        bucket_duration, IsEmpty(), _))
        .WillOnce(Return(std::vector<float>{2, 3, 2}));

    // Second uma feature should be the value histogram. The timestamp is set to
    // four different buckets.
    EXPECT_CALL(*feature_aggregator_,
                Process(proto::SignalType::HISTOGRAM_VALUE,
                        base::HashMetricName(histogram_value_name),
                        proto::Aggregation::BUCKETED_COUNT_BOOLEAN, 5,
                        StartTime(bucket_duration, 5), clock_.Now(),
                        bucket_duration, IsEmpty(), _))
        .WillOnce(Return(std::vector<float>{0, 1, 1, 1, 1}));
  }

  // The input tensor should contain all values flattened to a single vector.
  ExpectProcessedFeatureList(false,
                             ModelProvider::Request{2, 3, 2, 0, 1, 1, 1, 1});
}

TEST_P(FeatureListQueryProcessorTest, SingleUmaOutputWithObservationTime) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

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
  std::vector<SignalDatabase::DbEntry> user_action_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(output_user_action_name),
          .time = start_time,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(output_user_action_name),
          .time = start_time,
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(output_user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(output_user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(output_user_action_name),
          .time = clock_.Now(),
          .value = 0},
  };
  AddSamplesToSqlDb(user_action_samples);
  if (!use_sql_db) {
    // Without observation time.
    EXPECT_CALL(*signal_database_, GetAllSamples())
        .Times(1)
        .WillRepeatedly(Return(&user_action_samples));
    EXPECT_CALL(*feature_aggregator_,
                Process(proto::SignalType::USER_ACTION,
                        base::HashMetricName(output_user_action_name),
                        proto::Aggregation::COUNT, 2, start_time,
                        prediction_time, bucket_duration, IsEmpty(), _))
        .Times(1)
        .WillRepeatedly(Return(std::vector<float>{2}));
  }

  // Without observation time, output contains {2}
  ExpectProcessedFeatureList(
      false, ModelProvider::Request(), ModelProvider::Response{2},
      prediction_time, base::Time(),
      FeatureListQueryProcessor::ProcessOption::kOutputsOnly);

  if (!use_sql_db) {
    // With observation time.
    EXPECT_CALL(*signal_database_, GetAllSamples())
        .Times(1)
        .WillRepeatedly(Return(&user_action_samples));
    EXPECT_CALL(*feature_aggregator_,
                Process(proto::SignalType::USER_ACTION,
                        base::HashMetricName(output_user_action_name),
                        proto::Aggregation::COUNT, 2, prediction_time,
                        observation_time, bucket_duration, IsEmpty(), _))
        .Times(1)
        .WillRepeatedly(Return(std::vector<float>{3}));
  }

  // With observation time, output contains {3}
  ExpectProcessedFeatureList(
      false, ModelProvider::Request(), ModelProvider::Response{3},
      prediction_time, observation_time,
      FeatureListQueryProcessor::ProcessOption::kOutputsOnly);
}

class FeatureListQueryProcessorNoDbCacheTest
    : public FeatureListQueryProcessorTest {
 public:
  void InitFeatureList() override {
    feature_list_.InitAndDisableFeature(
        features::kSegmentationPlatformSignalDbCache);
  }
};
INSTANTIATE_TEST_SUITE_P(SqlDbAvailability,
                         FeatureListQueryProcessorNoDbCacheTest,
                         testing::Values(true, false));

TEST_P(FeatureListQueryProcessorNoDbCacheTest, MultipleUmaFeaturesWithOutputs) {
  const bool use_sql_db = GetParam();
  CreateFeatureListQueryProcessor(use_sql_db);

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
  std::vector<SignalDatabase::DbEntry> user_action_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now(),
          .value = 0},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::USER_ACTION,
          .name_hash = base::HashMetricName(user_action_name),
          .time = clock_.Now(),
          .value = 0},
  };
  AddSamplesToSqlDb(user_action_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_,
                GetSamples(proto::SignalType::USER_ACTION,
                           base::HashMetricName(user_action_name),
                           StartTime(bucket_duration, 2), clock_.Now(), _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<4>(user_action_samples));
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::USER_ACTION,
                base::HashMetricName(user_action_name),
                proto::Aggregation::COUNT, 2, StartTime(bucket_duration, 2),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{3}));
  }

  std::vector<SignalDatabase::DbEntry> histogram_value_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_VALUE,
          .name_hash = base::HashMetricName(histogram_value_name),
          .time = clock_.Now(),
          .value = 3},
  };
  AddSamplesToSqlDb(histogram_value_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_,
                GetSamples(proto::SignalType::HISTOGRAM_VALUE,
                           base::HashMetricName(histogram_value_name),
                           StartTime(bucket_duration, 3), clock_.Now(), _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<4>(histogram_value_samples));
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_VALUE,
                base::HashMetricName(histogram_value_name),
                proto::Aggregation::SUM, 3, StartTime(bucket_duration, 3),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{6}));
  }

  // Third uma feature should be the enum histogram.
  std::vector<SignalDatabase::DbEntry> histogram_enum_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 3},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(histogram_enum_name),
          .time = clock_.Now(),
          .value = 4},
  };
  AddSamplesToSqlDb(histogram_enum_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_,
                GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                           base::HashMetricName(histogram_enum_name),
                           StartTime(bucket_duration, 4), clock_.Now(), _))
        .Times(2)
        .WillRepeatedly(RunOnceCallbackRepeatedly<4>(histogram_enum_samples));
    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_ENUM,
                base::HashMetricName(histogram_enum_name),
                proto::Aggregation::COUNT, 4, StartTime(bucket_duration, 4),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{4}));
  }

  // Output is also enum histogram
  std::vector<SignalDatabase::DbEntry> output_histogram_enum_samples{
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 1},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 2},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 3},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 4},
      SignalDatabase::DbEntry{
          .type = proto::SignalType::HISTOGRAM_ENUM,
          .name_hash = base::HashMetricName(output_histogram_enum_name),
          .time = clock_.Now(),
          .value = 5},
  };
  AddSamplesToSqlDb(output_histogram_enum_samples);
  if (!use_sql_db) {
    EXPECT_CALL(*signal_database_,
                GetSamples(proto::SignalType::HISTOGRAM_ENUM,
                           base::HashMetricName(output_histogram_enum_name),
                           StartTime(bucket_duration, 5), clock_.Now(), _))
        .Times(2)
        .WillRepeatedly(
            RunOnceCallbackRepeatedly<4>(output_histogram_enum_samples));

    EXPECT_CALL(
        *feature_aggregator_,
        Process(proto::SignalType::HISTOGRAM_ENUM,
                base::HashMetricName(output_histogram_enum_name),
                proto::Aggregation::COUNT, 5, StartTime(bucket_duration, 5),
                clock_.Now(), bucket_duration, IsEmpty(), _))
        .Times(2)
        .WillRepeatedly(Return(std::vector<float>{5}));
  }

  // The input tensor should contain all three values: 3, 6, and 4.
  ExpectProcessedFeatureList(false, ModelProvider::Request{3, 6, 4});

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

}  // namespace segmentation_platform::processing
