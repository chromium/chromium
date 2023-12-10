// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/custom_input_processor.h"
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/execution/processing/processing_utils.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::processing {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

class MockInputDelegate : public InputDelegate {
 public:
  MOCK_METHOD3(Process,
               void(const proto::CustomInput& input,
                    FeatureProcessorState& feature_processor_state,
                    ProcessedCallback callback));
};

}  // namespace

class CustomInputProcessorTest : public testing::Test {
 public:
  CustomInputProcessorTest() = default;
  ~CustomInputProcessorTest() override = default;

  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    feature_processor_state_ = std::make_unique<FeatureProcessorState>();
    custom_input_processor_sql_ = std::make_unique<CustomInputProcessor>(
        clock_.Now(), &input_delegate_holder_);
  }

  void TearDown() override {
    feature_processor_state_.reset();
    custom_input_processor_sql_.reset();
  }

  Data CreateCustomInputData(
      int tensor_length,
      proto::CustomInput::FillPolicy fill_policy,
      const std::vector<float>& default_values,
      const std::vector<std::pair<std::string, std::string>>& additional_args) {
    proto::InputFeature input;
    proto::CustomInput* custom_input = input.mutable_custom_input();
    custom_input->set_fill_policy(fill_policy);
    custom_input->set_tensor_length(tensor_length);

    // Set default values.
    for (float default_value : default_values)
      custom_input->add_default_value(default_value);

    // Add additional arguments.
    custom_input->mutable_additional_args()->insert(additional_args.begin(),
                                                    additional_args.end());
    return Data(input);
  }

  void ExpectProcessedCustomInput(
      base::flat_map<int, Data>&& data,
      bool expected_error,
      const base::flat_map<int, QueryProcessor::Tensor>& expected_result) {
    feature_processor_state_ = std::make_unique<FeatureProcessorState>();
    ExpectProcessedCustomInput(std::move(data), *feature_processor_state_,
                               expected_error, expected_result);
  }

  void ExpectProcessedCustomInput(
      base::flat_map<int, Data>&& data,
      FeatureProcessorState& feature_processor_state,
      bool expected_error,
      const base::flat_map<int, QueryProcessor::Tensor>& expected_result) {
    std::unique_ptr<CustomInputProcessor> custom_input_processor =
        std::make_unique<CustomInputProcessor>(std::move(data), clock_.Now(),
                                               &input_delegate_holder_);

    base::RunLoop loop;
    custom_input_processor->Process(
        feature_processor_state,
        base::BindOnce(
            &CustomInputProcessorTest::OnProcessingFinishedCallback<int>,
            base::Unretained(this), loop.QuitClosure(), expected_error,
            expected_result, feature_processor_state.GetWeakPtr()));
    loop.Run();
  }

  template <typename IndexType>
  void ExpectProcessedCustomInputsForSql(
      const base::flat_map<IndexType, proto::CustomInput>& data,
      bool expected_error,
      const base::flat_map<IndexType, QueryProcessor::Tensor>&
          expected_result) {
    base::RunLoop loop;
    custom_input_processor_sql_->ProcessIndexType<IndexType>(
        data, *feature_processor_state_,
        std::make_unique<base::flat_map<IndexType, Tensor>>(),
        base::BindOnce(
            &CustomInputProcessorTest::OnProcessingFinishedCallback<IndexType>,
            base::Unretained(this), loop.QuitClosure(), expected_error,
            expected_result, feature_processor_state_->GetWeakPtr()));
    loop.Run();
  }

  template <typename IndexType>
  void OnProcessingFinishedCallback(
      base::RepeatingClosure closure,
      bool expected_error,
      const base::flat_map<IndexType, QueryProcessor::Tensor>& expected_result,
      base::WeakPtr<FeatureProcessorState> feature_processor_state,
      base::flat_map<IndexType, QueryProcessor::Tensor> result) {
    EXPECT_EQ(expected_error, feature_processor_state->error());
    EXPECT_EQ(expected_result, result);
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::SimpleTestClock clock_;
  InputDelegateHolder input_delegate_holder_;
  std::unique_ptr<FeatureProcessorState> feature_processor_state_;
  std::unique_ptr<CustomInputProcessor> custom_input_processor_sql_;
};

TEST_F(CustomInputProcessorTest, IntTypeIndex) {
  using IndexType = int;
  IndexType index = 0;
  base::flat_map<IndexType, proto::CustomInput> data;
  data.emplace(index, CreateCustomInputData(
                          1, proto::CustomInput::FILL_PREDICTION_TIME, {}, {})
                          .input_feature->custom_input());

  base::flat_map<IndexType, QueryProcessor::Tensor> expected_result;
  expected_result[index] = {ProcessedValue(clock_.Now())};
  ExpectProcessedCustomInputsForSql<IndexType>(data, /*expected_error=*/false,
                                               expected_result);
}

TEST_F(CustomInputProcessorTest, IntPairTypeIndex) {
  using IndexType = std::pair<int, int>;
  IndexType index = std::make_pair(0, 0);
  base::flat_map<IndexType, proto::CustomInput> data;
  data.emplace(index, CreateCustomInputData(
                          1, proto::CustomInput::FILL_PREDICTION_TIME, {}, {})
                          .input_feature->custom_input());

  base::flat_map<IndexType, QueryProcessor::Tensor> expected_result;
  expected_result[index] = {ProcessedValue(clock_.Now())};
  ExpectProcessedCustomInputsForSql<IndexType>(data, /*expected_error=*/false,
                                               expected_result);
}

TEST_F(CustomInputProcessorTest, DefaultValueCustomInput) {
  // Create custom inputs data.
  int index = 0;
  base::flat_map<int, Data> data;
  data.emplace(index,
               CreateCustomInputData(2, proto::CustomInput::UNKNOWN_FILL_POLICY,
                                     {1, 2}, {}));

  // Set expected tensor result.
  base::flat_map<int, QueryProcessor::Tensor> expected_result;
  expected_result[index] = {ProcessedValue(static_cast<float>(1)),
                            ProcessedValue(static_cast<float>(2))};

  // Process the custom inputs and verify using expected result.
  ExpectProcessedCustomInput(std::move(data), /*expected_error=*/false,
                             expected_result);
}

TEST_F(CustomInputProcessorTest, PredictionTimeCustomInput) {
  // Create custom inputs data.
  int index = 0;
  base::flat_map<int, Data> data;
  data.emplace(index, CreateCustomInputData(
                          1, proto::CustomInput::FILL_PREDICTION_TIME, {}, {}));

  // Set expected tensor result.
  base::flat_map<int, QueryProcessor::Tensor> expected_result;
  expected_result[index] = {ProcessedValue(clock_.Now())};

  // Process the custom inputs and verify using expected result.
  ExpectProcessedCustomInput(std::move(data), /*expected_error=*/false,
                             expected_result);
}

TEST_F(CustomInputProcessorTest, FromInputContext) {
  // Create custom input data.
  base::flat_map<int, Data> data;
  proto::InputFeature input;
  proto::CustomInput* custom_input = input.mutable_custom_input();
  custom_input->set_name("test_input");
  custom_input->set_fill_policy(proto::CustomInput::FILL_FROM_INPUT_CONTEXT);
  custom_input->set_tensor_length(1);
  data.emplace(0, Data(input));

  std::unique_ptr<FeatureProcessorState> feature_processor_state =
      std::make_unique<FeatureProcessorState>();
  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace("test_input", 0.6f);
  feature_processor_state->set_input_context_for_testing(input_context);

  // Set expected tensor result.
  base::flat_map<int, QueryProcessor::Tensor> expected_result;
  expected_result[0] = {ProcessedValue(0.6f)};

  // Process the custom inputs and verify using expected result.
  ExpectProcessedCustomInput(std::move(data), *feature_processor_state,
                             /*expected_error=*/false, expected_result);
}

TEST_F(CustomInputProcessorTest,
       FromInputContextUsesAdditionalArgNameIfPresent) {
  // Create custom input data.
  base::flat_map<int, Data> data;
  proto::InputFeature input;
  proto::CustomInput* custom_input = input.mutable_custom_input();
  custom_input->set_name("test_input");
  (*custom_input->mutable_additional_args())["name"] = "test_arg";
  custom_input->set_fill_policy(proto::CustomInput::FILL_FROM_INPUT_CONTEXT);
  custom_input->set_tensor_length(1);
  data.emplace(0, Data(input));

  std::unique_ptr<FeatureProcessorState> feature_processor_state =
      std::make_unique<FeatureProcessorState>();
  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace("test_arg", 0.6f);
  feature_processor_state->set_input_context_for_testing(input_context);

  // Set expected tensor result.
  base::flat_map<int, QueryProcessor::Tensor> expected_result;
  expected_result[0] = {ProcessedValue(0.6f)};

  // Process the custom inputs and verify using expected result.
  ExpectProcessedCustomInput(std::move(data), *feature_processor_state,
                             /*expected_error=*/false, expected_result);
}

TEST_F(CustomInputProcessorTest, TimeRangeBeforePredictionCustomInput) {
  // Create custom inputs data.
  int index = 0;
  base::flat_map<int, Data> data;
  data.emplace(index, CreateCustomInputData(
                          2, proto::CustomInput::TIME_RANGE_BEFORE_PREDICTION,
                          {}, {{"bucket_count", "1"}}));

  // Set expected tensor result.
  base::flat_map<int, QueryProcessor::Tensor> expected_result;
  expected_result[index] = {ProcessedValue(clock_.Now() - base::Days(1)),
                            ProcessedValue(clock_.Now())};

  // Process the custom inputs and verify using expected result.
  ExpectProcessedCustomInput(std::move(data), /*expected_error=*/false,
                             expected_result);
}

TEST_F(CustomInputProcessorTest, InvalidTimeRangeBeforePredictionCustomInput) {
  // Create custom inputs data.
  int index = 0;
  base::flat_map<int, Data> data;
  data.emplace(
      index, CreateCustomInputData(
                 2, proto::CustomInput::TIME_RANGE_BEFORE_PREDICTION, {}, {}));

  // Set expected tensor result.
  base::flat_map<int, QueryProcessor::Tensor> expected_result;

  // Process the custom inputs and verify using expected result.
  ExpectProcessedCustomInput(std::move(data), /*expected_error=*/true,
                             expected_result);
}

TEST_F(CustomInputProcessorTest, InputDelegateFailure) {
  auto moved_delegate = std::make_unique<MockInputDelegate>();
  MockInputDelegate* delegate = moved_delegate.get();
  input_delegate_holder_.SetDelegate(proto::CustomInput::PRICE_TRACKING_HINTS,
                                     std::move(moved_delegate));

  int index = 0;
  base::flat_map<int, Data> data;
  data.emplace(index, CreateCustomInputData(
                          2, proto::CustomInput::PRICE_TRACKING_HINTS, {}, {}));

  EXPECT_CALL(*delegate, Process(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, Tensor()));
  base::flat_map<int, QueryProcessor::Tensor> expected_result;
  ExpectProcessedCustomInput(std::move(data), /*expected_error=*/true,
                             expected_result);
}

TEST_F(CustomInputProcessorTest, InputDelegate) {
  auto moved_delegate = std::make_unique<MockInputDelegate>();
  MockInputDelegate* delegate = moved_delegate.get();
  input_delegate_holder_.SetDelegate(proto::CustomInput::PRICE_TRACKING_HINTS,
                                     std::move(moved_delegate));

  int index = 0;
  base::flat_map<int, Data> data;
  data.emplace(index, CreateCustomInputData(
                          2, proto::CustomInput::PRICE_TRACKING_HINTS, {}, {}));

  Tensor result{ProcessedValue(1), ProcessedValue(2)};
  EXPECT_CALL(*delegate, Process(_, _, _))
      .WillOnce(RunOnceCallback<2>(false, result));
  base::flat_map<int, QueryProcessor::Tensor> expected_result{{index, result}};
  ExpectProcessedCustomInput(std::move(data), /*expected_error=*/false,
                             expected_result);
}

TEST_F(CustomInputProcessorTest, MultipleFillTypesCustomInputs) {
  auto moved_delegate = std::make_unique<MockInputDelegate>();
  MockInputDelegate* delegate = moved_delegate.get();
  input_delegate_holder_.SetDelegate(proto::CustomInput::PRICE_TRACKING_HINTS,
                                     std::move(moved_delegate));
  EXPECT_CALL(*delegate, Process(_, _, _))
      .WillOnce(RunOnceCallback<2>(false, Tensor{ProcessedValue(1)}));

  // Create custom inputs data.
  base::flat_map<int, Data> data;
  data.emplace(0, CreateCustomInputData(
                      1, proto::CustomInput::FILL_PREDICTION_TIME, {}, {}));
  data.emplace(1, CreateCustomInputData(
                      2, proto::CustomInput::UNKNOWN_FILL_POLICY, {1, 2}, {}));
  data.emplace(2, CreateCustomInputData(
                      1, proto::CustomInput::UNKNOWN_FILL_POLICY, {3}, {}));
  data.emplace(3, CreateCustomInputData(
                      1, proto::CustomInput::PRICE_TRACKING_HINTS, {}, {}));

  // Set expected tensor result.
  base::flat_map<int, QueryProcessor::Tensor> expected_result;
  expected_result[0] = {ProcessedValue(clock_.Now())};
  expected_result[1] = {ProcessedValue(static_cast<float>(1)),
                        ProcessedValue(static_cast<float>(2))};
  expected_result[2] = {ProcessedValue(static_cast<float>(3))};
  expected_result[3] = {ProcessedValue(1)};

  // Process the custom inputs and verify using expected result.
  ExpectProcessedCustomInput(std::move(data), /*expected_error=*/false,
                             expected_result);
}

TEST_F(CustomInputProcessorTest, ProcessOsVersionString) {
  EXPECT_EQ(6, processing::ProcessOsVersionString("5.0.99"));
  EXPECT_EQ(6, processing::ProcessOsVersionString("6"));

  EXPECT_EQ(7, processing::ProcessOsVersionString("6.0.99"));
  EXPECT_EQ(7, processing::ProcessOsVersionString("7"));
  EXPECT_EQ(6, processing::ProcessOsVersionString("6.0.34"));

  EXPECT_EQ(8, processing::ProcessOsVersionString("7.0.99"));
  EXPECT_EQ(8, processing::ProcessOsVersionString("8"));

  EXPECT_EQ(9, processing::ProcessOsVersionString("8.0.99"));
  EXPECT_EQ(9, processing::ProcessOsVersionString("9"));

  EXPECT_EQ(10, processing::ProcessOsVersionString("9.0.99"));
  EXPECT_EQ(10, processing::ProcessOsVersionString("10"));
  EXPECT_EQ(10, processing::ProcessOsVersionString("10.0.34"));

  EXPECT_EQ(11, processing::ProcessOsVersionString("10.0.99"));
  EXPECT_EQ(11, processing::ProcessOsVersionString("11"));

  EXPECT_EQ(12, processing::ProcessOsVersionString("12"));
  EXPECT_EQ(13, processing::ProcessOsVersionString("13"));

  EXPECT_EQ(4, processing::ProcessOsVersionString("4"));
}

}  // namespace segmentation_platform::processing
