// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/mock_autofill_ai_model_cache.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
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
        &model_cache_, &model_executor_, logs_uploader_.get());
  }

  AutofillAiModelExecutor* engine() { return engine_.get(); }

  MockAutofillAiModelCache& model_cache() { return model_cache_; }

  optimization_guide::MockOptimizationGuideModelExecutor* model_executor() {
    return &model_executor_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_env_;
  TestingPrefServiceSimple local_state_;
  MockAutofillAiModelCache model_cache_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      model_executor_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_uploader_;
  std::unique_ptr<AutofillAiModelExecutor> engine_;
};

TEST_F(AutofillAiModelExecutorImplTest, ValidResponse) {
  const FormData form;
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
  EXPECT_CALL(model_cache(), Update(CalculateFormSignature(form), _, _));

  base::test::TestFuture<std::optional<Predictions>> test_future;
  engine()->GetPredictions(FormData(), test_future.GetCallback());
  EXPECT_TRUE(test_future.Get<0>().has_value());
}

// Tests that if there is an ongoing request with the same form signature, then
// GetPredictions will return immediately without result. However, queries for
// forms with different signatures will still be processed.
TEST_F(AutofillAiModelExecutorImplTest, OngoingRequestWithSameSignature) {
  // Two forms with different signatures.
  FormData form1;
  FormData form2;
  form2.set_name(u"some name");
  ASSERT_NE(CalculateFormSignature(form1), CalculateFormSignature(form2));

  OptimizationGuideModelExecutionResultCallback model_callback1;
  OptimizationGuideModelExecutionResultCallback model_callback2;
  // We only expect two calls to the model even though `GetPredictions` is
  // called three times.
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .Times(2)
      .WillOnce(MoveArg<3>(&model_callback1))
      .WillOnce(MoveArg<3>(&model_callback2));

  base::test::TestFuture<std::optional<Predictions>> test_future1;
  base::test::TestFuture<std::optional<Predictions>> test_future2;
  engine()->GetPredictions(form1, test_future1.GetCallback());
  EXPECT_FALSE(test_future1.IsReady());

  // We expect this call not to trigger a run.
  engine()->GetPredictions(form1, test_future2.GetCallback());
  EXPECT_FALSE(test_future2.Get<0>().has_value());

  // The simulated model call for a different form runs immediately and
  // completes successfully.
  base::test::TestFuture<std::optional<Predictions>> test_future3;
  engine()->GetPredictions(form2, test_future3.GetCallback());
  ASSERT_TRUE(model_callback2);
  std::move(model_callback2)
      .Run(OptimizationGuideModelExecutionResult(
               optimization_guide::AnyWrapProto(AutofillAiTypeResponse()),
               /*execution_info=*/nullptr),
           /*log_entry=*/nullptr);
  EXPECT_TRUE(test_future3.Get<0>().has_value());

  // Now simulate responding to the first call.
  ASSERT_TRUE(model_callback1);
  std::move(model_callback1)
      .Run(OptimizationGuideModelExecutionResult(
               optimization_guide::AnyWrapProto(AutofillAiTypeResponse()),
               /*execution_info=*/nullptr),
           /*log_entry=*/nullptr);
  EXPECT_TRUE(test_future1.Get<0>().has_value());
}

TEST_F(AutofillAiModelExecutorImplTest, ModelError) {
  const FormData form;
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
  EXPECT_CALL(model_cache(), Update(CalculateFormSignature(form), _, _));

  base::test::TestFuture<std::optional<Predictions>> test_future;
  engine()->GetPredictions(form, test_future.GetCallback());
  EXPECT_FALSE(test_future.Get<0>().has_value());
}

TEST_F(AutofillAiModelExecutorImplTest, WrongTypeReturned) {
  const FormData form;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::proto::Any(), /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  EXPECT_CALL(model_cache(), Update(CalculateFormSignature(form), _, _));

  base::test::TestFuture<std::optional<Predictions>> test_future;
  engine()->GetPredictions(form, test_future.GetCallback());
  EXPECT_FALSE(test_future.Get<0>().has_value());
}

}  // namespace
}  // namespace autofill
