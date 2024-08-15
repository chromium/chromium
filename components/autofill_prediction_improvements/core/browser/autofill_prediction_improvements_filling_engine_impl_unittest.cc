// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {
namespace {

using ::testing::_;
using ::testing::An;

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
  optimization_guide::proto::FormFieldData* filled_field =
      response.mutable_form_data()
          ->add_filled_form_field_data()
          ->mutable_field_data();
  filled_field->set_field_label("label");
  filled_field->set_field_value("value");
  optimization_guide::proto::FormFieldData* not_in_original_form_field =
      response.mutable_form_data()
          ->add_filled_form_field_data()
          ->mutable_field_data();
  not_in_original_form_field->set_field_label("notinform");
  not_in_original_form_field->set_field_value("doesntmatter");
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

  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  autofill::FormFieldData form_field_data2;
  form_field_data2.set_label(u"notinresponseandnotfilled");
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data2});
  optimization_guide::proto::AXTreeUpdate ax_tree;
  base::test::TestFuture<base::expected<autofill::FormData, bool>> test_future;
  engine()->GetPredictions(form_data, ax_tree, test_future.GetCallback());

  base::expected<autofill::FormData, bool> form_data_or_err =
      test_future.Take();
  EXPECT_TRUE(form_data_or_err.has_value());
  EXPECT_EQ(2u, form_data_or_err->fields().size());
  autofill::FormFieldData filled_field_response = form_data_or_err->fields()[0];
  EXPECT_EQ(u"label", filled_field_response.label());
  EXPECT_EQ(u"value", filled_field_response.value());
  autofill::FormFieldData filled_field_response2 =
      form_data_or_err->fields()[1];
  EXPECT_EQ(u"notinresponseandnotfilled", filled_field_response2.label());
  EXPECT_TRUE(filled_field_response2.value().empty());
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
  base::test::TestFuture<base::expected<autofill::FormData, bool>> test_future;
  engine()->GetPredictions(form_data, ax_tree, test_future.GetCallback());

  base::expected<autofill::FormData, bool> form_data_or_err =
      test_future.Take();
  EXPECT_TRUE(form_data_or_err.has_value());
  EXPECT_TRUE(form_data_or_err.has_value());
  EXPECT_EQ(1u, form_data_or_err->fields().size());
  autofill::FormFieldData filled_field_response = form_data_or_err->fields()[0];
  EXPECT_EQ(u"label", filled_field_response.label());
  EXPECT_TRUE(filled_field_response.value().empty());
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
  base::test::TestFuture<base::expected<autofill::FormData, bool>> test_future;
  engine()->GetPredictions(form_data, ax_tree, test_future.GetCallback());

  base::expected<autofill::FormData, bool> form_data_or_err =
      test_future.Take();
  EXPECT_FALSE(form_data_or_err.has_value());
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
  base::test::TestFuture<base::expected<autofill::FormData, bool>> test_future;
  engine()->GetPredictions(form_data, ax_tree, test_future.GetCallback());

  base::expected<autofill::FormData, bool> form_data_or_err =
      test_future.Take();
  EXPECT_FALSE(form_data_or_err.has_value());
}

}  // namespace
}  // namespace autofill_prediction_improvements
