// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/custom_input_processor.h"
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class CustomInputProcessorTest : public testing::Test {
 public:
  CustomInputProcessorTest() = default;
  ~CustomInputProcessorTest() override = default;

  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    feature_processor_state_ = std::make_unique<FeatureProcessorState>(
        base::Time(), base::TimeDelta(),
        OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN, nullptr,
        base::BindOnce([](bool, const std::vector<float>&) {}));
    custom_input_processor_sql_ =
        std::make_unique<CustomInputProcessor>(clock_.Now());
  }

  void TearDown() override {
    feature_processor_state_.reset();
    custom_input_processor_sql_.reset();
  }

  proto::CustomInput CreateCustomInputQuery(
      size_t tensor_length,
      proto::CustomInput::FillPolicy fill_policy,
      const std::vector<float>& default_values) {
    proto::CustomInput custom_input;
    custom_input.set_fill_policy(fill_policy);
    custom_input.set_tensor_length(tensor_length);
    for (float default_value : default_values)
      custom_input.add_default_value(default_value);
    return custom_input;
  }

  template <typename IndexType>
  void ExpectProcessedCustomInputs(
      const base::flat_map<IndexType, proto::CustomInput>& data,
      bool expected_error,
      const base::flat_map<IndexType, QueryProcessor::Tensor>&
          expected_result) {
    base::RunLoop loop;
    custom_input_processor_sql_->ProcessIndexType<IndexType>(
        data, std::move(feature_processor_state_),
        base::BindOnce(
            &CustomInputProcessorTest::OnProcessingFinishedCallback<IndexType>,
            base::Unretained(this), loop.QuitClosure(), expected_error,
            expected_result));
    loop.Run();
  }

  template <typename IndexType>
  void OnProcessingFinishedCallback(
      base::RepeatingClosure closure,
      bool expected_error,
      const base::flat_map<IndexType, QueryProcessor::Tensor>& expected_result,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      base::flat_map<IndexType, QueryProcessor::Tensor> result) {
    EXPECT_EQ(expected_error, feature_processor_state->error());
    EXPECT_EQ(expected_result, result);
    std::move(closure).Run();
  }

 protected:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FeatureProcessorState> feature_processor_state_;
  std::unique_ptr<CustomInputProcessor> custom_input_processor_sql_;
};

TEST_F(CustomInputProcessorTest, IntTypeIndex) {
  using IndexType = int;
  IndexType index = 0;
  base::flat_map<IndexType, proto::CustomInput> data;
  data[index] =
      CreateCustomInputQuery(1, proto::CustomInput::FILL_PREDICTION_TIME, {});

  base::flat_map<IndexType, QueryProcessor::Tensor> expected_result;
  expected_result[index] = {ProcessedValue(clock_.Now())};
  ExpectProcessedCustomInputs<IndexType>(data, /*expected_error=*/false,
                                         expected_result);
  task_environment_.RunUntilIdle();
}

TEST_F(CustomInputProcessorTest, IntPairTypeIndex) {
  using IndexType = std::pair<int, int>;
  IndexType index = std::make_pair(0, 0);
  base::flat_map<IndexType, proto::CustomInput> data;
  data[index] =
      CreateCustomInputQuery(1, proto::CustomInput::FILL_PREDICTION_TIME, {});

  base::flat_map<IndexType, QueryProcessor::Tensor> expected_result;
  expected_result[index] = {ProcessedValue(clock_.Now())};
  ExpectProcessedCustomInputs<IndexType>(data, /*expected_error=*/false,
                                         expected_result);
  task_environment_.RunUntilIdle();
}

}  // namespace segmentation_platform
