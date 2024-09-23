// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/sql_feature_processor.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;

namespace segmentation_platform::processing {

const auto kBoolType = proto::SqlFeature::BindValue::BOOL;

class SqlFeatureProcessorTest : public testing::Test {
 public:
  using CustomSqlQuery = UkmDatabase::CustomSqlQuery;

  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    feature_processor_state_ = std::make_unique<FeatureProcessorState>();
    ukm_database_ = std::make_unique<MockUkmDatabase>();
  }

  void TearDown() override {
    feature_processor_state_.reset();
    ukm_database_ = nullptr;
  }

  Data CreateSqlFeature(const std::string& sql,
                        const MetadataWriter::BindValues& custom_inputs) {
    proto::SegmentationModelMetadata metadata;
    MetadataWriter writer(&metadata);
    MetadataWriter::SqlFeature feature{.sql = sql.c_str()};
    writer.AddSqlFeature(feature, custom_inputs);
    return Data(metadata.input_features(0));
  }

  void ExpectQueryResult(
      SqlFeatureProcessor::QueryList&& data,
      const base::flat_map<SqlFeatureProcessor::FeatureIndex, CustomSqlQuery>&
          processed_queries,
      const SqlFeatureProcessor::IndexedTensors& result) {
    // Initialize the sql feature processor.
    std::unique_ptr<SqlFeatureProcessor> processor =
        std::make_unique<SqlFeatureProcessor>(std::move(data), clock_.Now(),
                                              &input_delegate_holder_,
                                              ukm_database_.get());

    EXPECT_CALL(*ukm_database_, RunReadOnlyQueries)
        .WillOnce(testing::Invoke(
            [&processed_queries, &result](
                const base::flat_map<SqlFeatureProcessor::FeatureIndex,
                                     CustomSqlQuery>& queries,
                MockUkmDatabase::QueryCallback callback) {
              EXPECT_EQ(processed_queries, queries);
              std::move(callback).Run(true, result);
            }));

    // Process the sql query.
    base::RunLoop loop;
    processor->Process(
        *feature_processor_state_,
        base::BindOnce(&SqlFeatureProcessorTest::OnProcessingFinishedCallback,
                       base::Unretained(this), loop.QuitClosure(), false,
                       result, feature_processor_state_->GetWeakPtr()));
    loop.Run();
  }

  void OnProcessingFinishedCallback(
      base::RepeatingClosure closure,
      bool expected_error,
      const SqlFeatureProcessor::IndexedTensors& expected_result,
      base::WeakPtr<FeatureProcessorState> feature_processor_state,
      SqlFeatureProcessor::IndexedTensors result) {
    EXPECT_EQ(expected_error, feature_processor_state->error());
    EXPECT_EQ(expected_result, result);
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_envåironment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::SimpleTestClock clock_;
  InputDelegateHolder input_delegate_holder_;
  std::unique_ptr<FeatureProcessorState> feature_processor_state_;
  std::unique_ptr<MockUkmDatabase> ukm_database_;
};

TEST_F(SqlFeatureProcessorTest, EmptyBindValues) {
  // Set up a single empty sql feature.
  SqlFeatureProcessor::QueryList data;
  constexpr char kSqlQuery[] = "some sql query";
  data.emplace(0, CreateSqlFeature(kSqlQuery, {}));

  // Construct the expected processed bind values based on the given data.
  base::flat_map<SqlFeatureProcessor::FeatureIndex, CustomSqlQuery>
      processed_queries;
  processed_queries[0] = CustomSqlQuery(kSqlQuery, {});

  // Construct a result to be returned when the correct processed queries are
  // sent.
  SqlFeatureProcessor::IndexedTensors result;
  result[0] = {ProcessedValue(1), ProcessedValue(2), ProcessedValue(3)};

  ExpectQueryResult(std::move(data), processed_queries, result);
}

TEST_F(SqlFeatureProcessorTest, SingleSqlFeatureWithBindValues) {
  // Set up a single empty sql feature.
  SqlFeatureProcessor::QueryList data;
  constexpr char kSqlQuery[] = "some sql query with three bind value ? ? ?";
  std::array<float, 3> default_values = {1, 2, 3};
  MetadataWriter::BindValue custom_input1{
      kBoolType,
      {.tensor_length = 1,
       .fill_policy = proto::CustomInput::FILL_PREDICTION_TIME}};
  MetadataWriter::BindValue custom_input2{
      kBoolType,
      {
          .tensor_length = 2,
          .fill_policy = proto::CustomInput::UNKNOWN_FILL_POLICY,
          .default_values_size = default_values.size(),
          .default_values = default_values.data(),
      }};
  data.emplace(0, CreateSqlFeature(kSqlQuery, {custom_input1, custom_input2}));

  // Construct the expected processed bind values based on the given data.
  base::flat_map<SqlFeatureProcessor::FeatureIndex, CustomSqlQuery>
      processed_queries;
  processed_queries[0] =
      CustomSqlQuery(kSqlQuery, {ProcessedValue(clock_.Now()),
                                 ProcessedValue(static_cast<float>(1)),
                                 ProcessedValue(static_cast<float>(2))});

  // Construct a result to be returned when the correct processed queries are
  // sent.
  SqlFeatureProcessor::IndexedTensors result;
  result[0] = {ProcessedValue(1), ProcessedValue(2), ProcessedValue(3)};

  ExpectQueryResult(std::move(data), processed_queries, result);
}

TEST_F(SqlFeatureProcessorTest, MultipleSqlFeatures) {
  // Set up a single empty sql feature.
  SqlFeatureProcessor::QueryList data;
  constexpr char kSqlQuery[] = "some sql query";
  constexpr char kSqlQueryWithBindValue[] =
      "some sql query with one bind value ?";
  MetadataWriter::BindValue custom_input1{
      kBoolType,
      {.tensor_length = 1,
       .fill_policy = proto::CustomInput::FILL_PREDICTION_TIME}};
  data.emplace(0, CreateSqlFeature(kSqlQueryWithBindValue, {custom_input1}));
  data.emplace(1, CreateSqlFeature(kSqlQuery, {}));
  data.emplace(2, CreateSqlFeature(kSqlQueryWithBindValue, {custom_input1}));

  // Construct the expected processed bind values based on the given data.
  base::flat_map<SqlFeatureProcessor::FeatureIndex, CustomSqlQuery>
      processed_queries;
  processed_queries[0] =
      CustomSqlQuery(kSqlQueryWithBindValue, {ProcessedValue(clock_.Now())});
  processed_queries[1] = CustomSqlQuery(kSqlQuery, {});
  processed_queries[2] =
      CustomSqlQuery(kSqlQueryWithBindValue, {ProcessedValue(clock_.Now())});

  // Construct a result to be returned when the correct processed queries are
  // sent.
  SqlFeatureProcessor::IndexedTensors result;
  result[0] = {ProcessedValue(1), ProcessedValue(2), ProcessedValue(3)};

  ExpectQueryResult(std::move(data), processed_queries, result);
}

}  // namespace segmentation_platform::processing
