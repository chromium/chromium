// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using Predictions = AutofillAiModelExecutor::Predictions;
using optimization_guide::OptimizationGuideModelExecutionError;
using optimization_guide::OptimizationGuideModelExecutionResult;
using optimization_guide::OptimizationGuideModelExecutionResultCallback;
using optimization_guide::proto::AutofillAiTypeResponse;
using ::testing::_;
using ::testing::An;

class AutofillAiModelExecutorImplTest : public testing::Test {
 public:
  void SetUp() override {
    logs_uploader_ = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(&local_state_);
    engine_ = std::make_unique<AutofillAiModelExecutorImpl>(
        &model_executor_, logs_uploader_.get());
  }

  AutofillAiModelExecutor* engine() { return engine_.get(); }

  optimization_guide::MockOptimizationGuideModelExecutor* model_executor() {
    return &model_executor_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_env_;
  TestingPrefServiceSimple local_state_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      model_executor_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_uploader_;
  std::unique_ptr<AutofillAiModelExecutor> engine_;
};

TEST_F(AutofillAiModelExecutorImplTest, ValidResponse) {
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(AutofillAiTypeResponse()),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  base::test::TestFuture<std::optional<Predictions>> test_future;
  engine()->GetPredictions(FormData(),
                           optimization_guide::proto::AXTreeUpdate(),
                           test_future.GetCallback());
  EXPECT_TRUE(test_future.Get<0>().has_value());
}

TEST_F(AutofillAiModelExecutorImplTest, ModelError) {
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              base::unexpected(
                  OptimizationGuideModelExecutionError::FromModelExecutionError(
                      OptimizationGuideModelExecutionError::
                          ModelExecutionError::kGenericFailure)),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  base::test::TestFuture<std::optional<Predictions>> test_future;
  engine()->GetPredictions(FormData(),
                           optimization_guide::proto::AXTreeUpdate(),
                           test_future.GetCallback());
  EXPECT_FALSE(test_future.Get<0>().has_value());
}

TEST_F(AutofillAiModelExecutorImplTest, WrongTypeReturned) {
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::proto::Any(), /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  base::test::TestFuture<std::optional<Predictions>> test_future;
  engine()->GetPredictions(FormData(),
                           optimization_guide::proto::AXTreeUpdate(),
                           test_future.GetCallback());
  EXPECT_FALSE(test_future.Get<0>().has_value());
}

}  // namespace
}  // namespace autofill
