// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {
namespace {

using Prediction = AutofillPredictionImprovementsFillingEngine::Prediction;
using PredictionsOrError =
    AutofillPredictionImprovementsFillingEngine::PredictionsOrError;
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
    const std::string& value) {
  optimization_guide::proto::FilledFormFieldData* filled_field =
      response.mutable_form_data()->add_filled_form_field_data();
  filled_field->mutable_field_data()->set_field_label(label);
  filled_field->set_normalized_label(normalized_label);
  optimization_guide::proto::PredictedValue* predicted_value =
      filled_field->add_predicted_values();
  predicted_value->set_value(value);
}

MATCHER_P(HasPrediction, expected_prediction, "") {
  EXPECT_THAT(arg, AllOf(Field("Prediction::value", &Prediction::value,
                               expected_prediction.value),
                         Field("Prediction::label", &Prediction::label,
                               expected_prediction.label),
                         Field("Prediction::select_option_text",
                               &Prediction::select_option_text,
                               expected_prediction.select_option_text)));
  return true;
}

class AutofillPredictionImprovementsFillingEngineImplTest
    : public testing::Test {
 public:
  void SetUp() override {
    user_annotations_service_ =
        std::make_unique<user_annotations::TestUserAnnotationsService>();
    engine_ = std::make_unique<AutofillPredictionImprovementsFillingEngineImpl>(
        &model_executor_, user_annotations_service_.get());
  }

  AutofillPredictionImprovementsFillingEngineImpl* engine() {
    return engine_.get();
  }

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
  std::unique_ptr<AutofillPredictionImprovementsFillingEngineImpl> engine_;
};

TEST_F(AutofillPredictionImprovementsFillingEngineImplTest, EndToEnd) {
  // Seed user annotations service with entries.
  optimization_guide::proto::UserAnnotationsEntry entry;
  entry.set_key("label");
  entry.set_value("value");
  user_annotations_service()->ReplaceAllEntries({entry});

  // Set up mock.
  optimization_guide::proto::FormsPredictionsResponse response;
  AddFieldToResponse(response, "label", "normalized label", "value");
  AddFieldToResponse(response, "empty", "", "");
  AddFieldToResponse(response, "notinform", "", "doesntmatter");
  AddFieldToResponse(response, "State", "", "33");
  AddFieldToResponse(
      response, "Country Code - response not in select options, not filled", "",
      "-2");
  AddFieldToResponse(response,
                     "Country - response equals selected value, not filled", "",
                     "2");
  AddFieldToResponse(response, "Field has value, not filled", "", "value");
  optimization_guide::proto::Any any;
  any.set_type_url(response.GetTypeName());
  response.SerializeToString(any.mutable_value());
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(any, /*log_entry=*/nullptr));

  autofill::test::FormDescription form_description = {
      .fields = {
          {.label = u"label"},
          {.label = u"not in response, not filled"},
          {.label = u"empty, not filled"},
          {.label = u"State",
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
  engine()->GetPredictions(form, ax_tree, test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  ASSERT_TRUE(predictions_or_error.has_value());
  EXPECT_THAT(
      predictions_or_error.value(),
      ElementsAre(
          Pair(form.fields()[0].global_id(),
               // Also tests that Prediction::label is set to the normalized
               // label if set and non-empty.
               HasPrediction(Prediction(u"value", u"normalized label"))),
          Pair(form.fields()[3].global_id(),
               // Also tests that Prediction::label falls back to the field
               // label if the normalized label is not set or empty.
               HasPrediction(Prediction(u"33", u"State", u"North Carolina")))));
}

TEST_F(AutofillPredictionImprovementsFillingEngineImplTest,
       NoUserAnnotationEntries) {
  // Seed user annotations service explicitly with no entries.
  user_annotations_service()->ReplaceAllEntries({});

  // Make sure model executor not called.
  EXPECT_CALL(*model_executor(), ExecuteModel(_, _, _)).Times(0);

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<PredictionsOrError, std::optional<std::string>>
      test_future;
  engine()->GetPredictions(form_data, ax_tree, test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  EXPECT_FALSE(predictions_or_error.has_value());
}

TEST_F(AutofillPredictionImprovementsFillingEngineImplTest,
       ModelExecutionError) {
  // Seed user annotations service with entries.
  optimization_guide::proto::UserAnnotationsEntry entry;
  entry.set_key("label");
  entry.set_value("value");
  user_annotations_service()->ReplaceAllEntries({entry});

  // Set up mock.
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          base::unexpected(
              optimization_guide::OptimizationGuideModelExecutionError::
                  FromModelExecutionError(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          ModelExecutionError::kGenericFailure)),
          /*log_entry=*/nullptr));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<PredictionsOrError, std::optional<std::string>>
      test_future;
  engine()->GetPredictions(form_data, ax_tree, test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  EXPECT_FALSE(predictions_or_error.has_value());
}

TEST_F(AutofillPredictionImprovementsFillingEngineImplTest,
       ModelExecutionWrongTypeReturned) {
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
          optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, _,
          An<optimization_guide::
                 OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(any, /*log_entry=*/nullptr));

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<PredictionsOrError, std::optional<std::string>>
      test_future;
  engine()->GetPredictions(form_data, ax_tree, test_future.GetCallback());

  const PredictionsOrError predictions_or_error =
      std::get<0>(test_future.Take());
  EXPECT_FALSE(predictions_or_error.has_value());
}

}  // namespace
}  // namespace autofill_prediction_improvements
