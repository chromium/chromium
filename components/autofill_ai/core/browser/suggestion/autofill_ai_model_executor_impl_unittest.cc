// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_test_utils.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {
namespace {

using Prediction = AutofillAiModelExecutor::Prediction;
using PredictionsOrError = AutofillAiModelExecutor::PredictionsOrError;
using ::testing::_;
using ::testing::AllOf;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pair;

void AddFieldToResponse(
    optimization_guide::proto::FormsPredictionsResponse& response,
    const std::string& label,
    const std::string& normalized_label,
    const std::string& value,
    int request_field_index = 0) {
  optimization_guide::proto::FilledFormFieldData* filled_field =
      response.mutable_form_data()->add_filled_form_field_data();
  filled_field->mutable_field_data()->set_field_label(label);
  filled_field->set_normalized_label(normalized_label);
  optimization_guide::proto::PredictedValue* predicted_value =
      filled_field->add_predicted_values();
  predicted_value->set_value(value);
  filled_field->set_request_field_index(request_field_index);
}

auto HasPrediction(Prediction expected_prediction) {
  return AllOf(
      Field("Prediction::value", &Prediction::value, expected_prediction.value),
      Field("Prediction::label", &Prediction::label, expected_prediction.label),
      Field("Prediction::select_option_text", &Prediction::select_option_text,
            expected_prediction.select_option_text));
}

class AutofillAiModelExecutorImplTest : public testing::Test {
 public:
  void SetUp() override {
    user_annotations_service_ =
        std::make_unique<user_annotations::TestUserAnnotationsService>();
    engine_ = std::make_unique<AutofillAiModelExecutorImpl>(
        &model_executor_, user_annotations_service_.get());
  }

  AutofillAiModelExecutorImpl* engine() { return engine_.get(); }

  optimization_guide::MockOptimizationGuideModelExecutor* model_executor() {
    return &model_executor_;
  }

  user_annotations::TestUserAnnotationsService* user_annotations_service() {
    return user_annotations_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
  testing::NiceMock<optimization_guide::MockOptimizationGuideModelExecutor>
      model_executor_;
  std::unique_ptr<user_annotations::TestUserAnnotationsService>
      user_annotations_service_;
  std::unique_ptr<AutofillAiModelExecutorImpl> engine_;
};

TEST_F(AutofillAiModelExecutorImplTest, EndToEnd) {
  // Seed user annotations service with entries.
  optimization_guide::proto::UserAnnotationsEntry entry;
  entry.set_key("label");
  entry.set_value("value");
  user_annotations_service()->ReplaceAllEntries({entry});

  // Set up mock.
  optimization_guide::proto::FormsPredictionsResponse response;
  AddFieldToResponse(response, "label", "normalized label", "value", 0);
  AddFieldToResponse(response, "empty", "", "", 2);
  AddFieldToResponse(response, "notinform", "", "doesntmatter");
  AddFieldToResponse(response, "State", "", "North Carolina", 3);
  AddFieldToResponse(
      response, "Country Code - response not in select options, not filled", "",
      "-2", 4);
  AddFieldToResponse(response,
                     "Country - response equals selected value, not filled", "",
                     "Spain", 5);
  AddFieldToResponse(response, "Field has value, not filled", "", "value", 6);
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(

              optimization_guide::AnyWrapProto(response),
              /*model_execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  autofill::test::FormDescription form_description = {
      .fields = {
          {.label = u"label"},
          {.label = u"not in response, not filled"},
          {.label = u"empty, not filled"},
          {.is_focusable = false,
           .label = u"State",
           .value = u"-1",
           .form_control_type = autofill::FormControlType::kSelectOne,
           .select_options = {{.value = u"-1", .text = u"Select state"},
                              {.value = u"33", .text = u"North Carolina"}}},
          {.label =
               u"Country Code - response not in select options, not filled",
           .value = u"-1",
           .form_control_type = autofill::FormControlType::kSelectOne,
           .select_options = {{.value = u"-1", .text = u"Select country code"},
                              {.value = u"+49", .text = u"Germany"}}},
          {.label = u"Country - response equals selected value, not filled",
           .value = u"2",
           .form_control_type = autofill::FormControlType::kSelectOne,
           .select_options = {{.value = u"1", .text = u"France"},
                              {.value = u"2", .text = u"Spain"}}},
          {.label = u"Field has value, not filled", .value = u"value"}}};
  autofill::FormData form = autofill::test::GetFormData(form_description);

  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<PredictionsOrError, std::optional<std::string>>
      test_future;
  engine()->GetPredictions(form, {}, {}, ax_tree, test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  ASSERT_TRUE(predictions_or_error.has_value());
  EXPECT_THAT(
      predictions_or_error.value(),
      ElementsAre(
          Pair(form.fields()[0].global_id(),
               // Also tests that Prediction::label is set to the normalized
               // label if set and non-empty.
               HasPrediction(Prediction(u"value", u"normalized label",
                                        /*is_focusable=*/true))),
          Pair(form.fields()[3].global_id(),
               // Also tests that Prediction::label falls back to the field
               // label if the normalized label is not set or empty.
               HasPrediction(Prediction(u"33", u"State", /*is_focusable=*/false,
                                        u"North Carolina")))));
}

TEST_F(AutofillAiModelExecutorImplTest, NoUserAnnotationEntries) {
  // Seed user annotations service explicitly with no entries.
  user_annotations_service()->ReplaceAllEntries({});

  // Make sure model executor not called.
  EXPECT_CALL(*model_executor(), ExecuteModel).Times(0);

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<PredictionsOrError, std::optional<std::string>>
      test_future;
  engine()->GetPredictions(form_data, {}, {}, ax_tree,
                           test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  EXPECT_FALSE(predictions_or_error.has_value());
}

TEST_F(AutofillAiModelExecutorImplTest, ModelExecutionError) {
  // Seed user annotations service with entries.
  optimization_guide::proto::UserAnnotationsEntry entry;
  entry.set_key("label");
  entry.set_value("value");
  user_annotations_service()->ReplaceAllEntries({entry});

  // Set up mock.
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionError(
                          optimization_guide::
                              OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure)),
              /*model_execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<PredictionsOrError, std::optional<std::string>>
      test_future;
  engine()->GetPredictions(form_data, {}, {}, ax_tree,
                           test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  EXPECT_FALSE(predictions_or_error.has_value());
}

TEST_F(AutofillAiModelExecutorImplTest, ModelExecutionWrongTypeReturned) {
  // Seed user annotations service with entries.
  optimization_guide::proto::UserAnnotationsEntry entry;
  entry.set_key("label");
  entry.set_value("value");
  user_annotations_service()->ReplaceAllEntries({entry});

  // Set up mock.
  optimization_guide::proto::Any any;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, _, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              any, /*model_execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<PredictionsOrError, std::optional<std::string>>
      test_future;
  engine()->GetPredictions(form_data, {}, {}, ax_tree,
                           test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  EXPECT_FALSE(predictions_or_error.has_value());
}

}  // namespace
}  // namespace autofill_ai
