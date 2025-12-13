// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"

#include <memory>
#include <optional>

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/mock_autofill_ai_model_cache.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::test::EqualsProto;
using FieldIdentifier = AutofillAiModelCache::FieldIdentifier;
using optimization_guide::OptimizationGuideModelExecutionError;
using optimization_guide::OptimizationGuideModelExecutionResult;
using optimization_guide::OptimizationGuideModelExecutionResultCallback;
using optimization_guide::proto::AutofillAiTypeRequest;
using optimization_guide::proto::AutofillAiTypeResponse;
using ::testing::_;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using MockOnModelExecutedCallback =
    base::MockCallback<base::OnceCallback<void(const FormGlobalId&)>>;

class AutofillAiModelExecutorImplTest : public testing::Test {
 public:
  AutofillAiModelExecutorImplTest() : mqls_uploader_(&local_state_) {
    optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        local_state_.registry());
    engine_ = std::make_unique<AutofillAiModelExecutorImpl>(
        &model_cache_, &model_executor_, &mqls_uploader_);
  }

  AutofillAiModelExecutor* engine() { return engine_.get(); }

  MockAutofillAiModelCache& model_cache() { return model_cache_; }

  optimization_guide::MockRemoteModelExecutor* model_executor() {
    return &model_executor_;
  }

  optimization_guide::TestModelQualityLogsUploaderService& mqls_uploader() {
    return mqls_uploader_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  test::AutofillUnitTestEnvironment autofill_test_env_;
  MockAutofillAiModelCache model_cache_;
  testing::NiceMock<optimization_guide::MockRemoteModelExecutor>
      model_executor_;
  optimization_guide::TestModelQualityLogsUploaderService mqls_uploader_;
  std::unique_ptr<AutofillAiModelExecutor> engine_;
};

TEST_F(AutofillAiModelExecutorImplTest, ValidResponse) {
  base::HistogramTester histogram_tester;
  const FormData form =
      test::GetFormData({.fields = {{.name = u"Passport number"}}});
  AutofillAiTypeResponse response;
  {
    auto* field_response = response.add_field_responses();
    field_response->set_field_type(PASSPORT_NUMBER);
    field_response->set_field_index(0);
  }

  MockOnModelExecutedCallback on_model_executed;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  EXPECT_CALL(
      model_cache(),
      Update(CalculateFormSignature(form), EqualsProto(response),
             ElementsAre(FieldIdentifier{
                 .signature = CalculateFieldSignatureForField(form.fields()[0]),
                 .rank_in_signature_group = 0})));
  EXPECT_CALL(on_model_executed, Run(form.global_id()));

  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      kUmaAutofillAiModelExecutionStatus,
      AutofillAiModelExecutionStatus::kSuccessNonEmptyResult, 1);
}

// Tests that if the field index of a prediction is out of bounds of the
// fields in the `FormData`, then nothing is written to the cache.
TEST_F(AutofillAiModelExecutorImplTest, FieldIndexOutOfBounds) {
  base::HistogramTester histogram_tester;
  const FormData form =
      test::GetFormData({.fields = {{.name = u"Passport number"}}});
  AutofillAiTypeResponse response;
  {
    auto* field_response = response.add_field_responses();
    field_response->set_field_type(PASSPORT_NUMBER);
    field_response->set_field_index(1);
  }

  MockOnModelExecutedCallback on_model_executed;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form),
                     EqualsProto(AutofillAiTypeResponse()), IsEmpty()));
  EXPECT_CALL(on_model_executed, Run(form.global_id()));

  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      kUmaAutofillAiModelExecutionStatus,
      AutofillAiModelExecutionStatus::kErrorInvalidFieldIndex, 1);
}

// Tests that if the field index of a prediction is negative, then nothing is
// written to the cache.
TEST_F(AutofillAiModelExecutorImplTest, FieldIndexNegative) {
  base::HistogramTester histogram_tester;
  const FormData form =
      test::GetFormData({.fields = {{.name = u"Passport number"}}});
  AutofillAiTypeResponse response;
  {
    auto* field_response = response.add_field_responses();
    field_response->set_field_type(PASSPORT_NUMBER);
    field_response->set_field_index(-1);
  }

  MockOnModelExecutedCallback on_model_executed;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form),
                     EqualsProto(AutofillAiTypeResponse()), IsEmpty()));
  EXPECT_CALL(on_model_executed, Run(form.global_id()));

  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      kUmaAutofillAiModelExecutionStatus,
      AutofillAiModelExecutionStatus::kErrorInvalidFieldIndex, 1);
}

// Tests that if there are duplicate field indices, then nothing is written
// into the cache.
TEST_F(AutofillAiModelExecutorImplTest, DuplicateFieldIndices) {
  base::HistogramTester histogram_tester;
  const FormData form =
      test::GetFormData({.fields = {{.name = u"Passport number"},
                                    {.name = u"Passport issuing country"}}});
  AutofillAiTypeResponse response;
  {
    auto* field_response = response.add_field_responses();
    field_response->set_field_type(PASSPORT_NUMBER);
    field_response->set_field_index(0);
  }
  {
    auto* field_response = response.add_field_responses();
    field_response->set_field_type(PASSPORT_ISSUING_COUNTRY);
    field_response->set_field_index(0);
  }

  MockOnModelExecutedCallback on_model_executed;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form),
                     EqualsProto(AutofillAiTypeResponse()), IsEmpty()));
  EXPECT_CALL(on_model_executed, Run(form.global_id()));

  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      kUmaAutofillAiModelExecutionStatus,
      AutofillAiModelExecutionStatus::kErrorInvalidFieldIndex, 1);
}

// Tests that if there is an ongoing request with the same form signature, then
// GetPredictions will return immediately without result. However, queries for
// forms with different signatures will still be processed.
TEST_F(AutofillAiModelExecutorImplTest, OngoingRequestWithSameSignature) {
  // Two forms with different signatures and two different responses.
  const FormData form1 =
      test::GetFormData({.fields = {{.name = u"Passport number"}}});
  AutofillAiTypeResponse response1;
  response1.add_field_responses()->set_field_type(PASSPORT_NUMBER);

  const FormData form2 =
      test::GetFormData({.fields = {{.name = u"First name"}}});
  AutofillAiTypeResponse response2;
  response2.add_field_responses()->set_field_type(NAME_FIRST);

  ASSERT_NE(CalculateFormSignature(form1), CalculateFormSignature(form2));

  OptimizationGuideModelExecutionResultCallback model_callback1;
  OptimizationGuideModelExecutionResultCallback model_callback2;
  // We only expect two calls to the model and to the cache even though
  // `GetPredictions` is called three times.
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .Times(2)
      .WillOnce(MoveArg<3>(&model_callback1))
      .WillOnce(MoveArg<3>(&model_callback2));
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form2), EqualsProto(response2),
                     ElementsAre(FieldIdentifier{
                         .signature =
                             CalculateFieldSignatureForField(form2.fields()[0]),
                         .rank_in_signature_group = 0})));
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form1), EqualsProto(response1),
                     ElementsAre(FieldIdentifier{
                         .signature =
                             CalculateFieldSignatureForField(form1.fields()[0]),
                         .rank_in_signature_group = 0})));

  engine()->GetPredictions(form1, base::DoNothing(), std::nullopt);

  // We expect this call not to trigger a run.
  engine()->GetPredictions(form1, base::DoNothing(), std::nullopt);

  // The simulated model call for a different form runs immediately and
  // completes successfully.
  engine()->GetPredictions(form2, base::DoNothing(), std::nullopt);
  ASSERT_TRUE(model_callback2);
  std::move(model_callback2)
      .Run(OptimizationGuideModelExecutionResult(
               optimization_guide::AnyWrapProto(response2),
               /*execution_info=*/nullptr),
           /*log_entry=*/nullptr);

  // Now simulate responding to the first call.
  ASSERT_TRUE(model_callback1);
  std::move(model_callback1)
      .Run(OptimizationGuideModelExecutionResult(
               optimization_guide::AnyWrapProto(response1),
               /*execution_info=*/nullptr),
           /*log_entry=*/nullptr);
}

// Tests that model errors are handled by writing an empty entry into the cache.
TEST_F(AutofillAiModelExecutorImplTest, ModelError) {
  base::HistogramTester histogram_tester;
  const FormData form;
  MockOnModelExecutedCallback on_model_executed;
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
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form),
                     EqualsProto(AutofillAiTypeResponse()), IsEmpty()));
  EXPECT_CALL(on_model_executed, Run(form.global_id()));

  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      kUmaAutofillAiModelExecutionStatus,
      AutofillAiModelExecutionStatus::kErrorServerError, 1);
}

// Tests that wrongly typed model responses are handled by writing an empty
// entry into the cache.
TEST_F(AutofillAiModelExecutorImplTest, WrongTypeReturned) {
  base::HistogramTester histogram_tester;
  const FormData form;
  MockOnModelExecutedCallback on_model_executed;
  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kFormsClassifications, _,
          _, An<OptimizationGuideModelExecutionResultCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::proto::Any(), /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form),
                     EqualsProto(AutofillAiTypeResponse()), IsEmpty()));
  EXPECT_CALL(on_model_executed, Run(form.global_id()));

  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      kUmaAutofillAiModelExecutionStatus,
      AutofillAiModelExecutionStatus::kErrorWrongResponseType, 1);
}

TEST_F(AutofillAiModelExecutorImplTest, MqlsUpload) {
  base::test::ScopedFeatureList features{
      optimization_guide::features::kFormsClassificationsMqlsLogging};

  const FormData form =
      test::GetFormData({.fields = {{.name = u"Passport number"}}});

  AutofillAiTypeRequest expected_request;
  optimization_guide::proto::FormData* stripped_form =
      expected_request.mutable_form_data();
  stripped_form->set_form_signature(*CalculateFormSignature(form));
  stripped_form->add_fields()->set_field_signature(
      *CalculateFieldSignatureForField(form.fields()[0]));
  AutofillAiTypeResponse response;
  optimization_guide::proto::FieldTypeResponse* field_response =
      response.add_field_responses();
  field_response->set_field_type(PASSPORT_NUMBER);
  field_response->set_field_index(0);

  MockOnModelExecutedCallback on_model_executed;
  EXPECT_CALL(*model_executor(), ExecuteModel)
      .WillOnce(base::test::RunOnceCallback<3>(
          OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));
  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
      uploaded_logs = mqls_uploader().uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 1u);
  const optimization_guide::proto::FormsClassificationsLoggingData& log =
      uploaded_logs[0]->forms_classifications();
  EXPECT_THAT(log.request(), EqualsProto(expected_request));
  EXPECT_THAT(log.response(), EqualsProto(response));
}

// Tests that no MQLS log is sent if the server returned with an error.
TEST_F(AutofillAiModelExecutorImplTest, NoMqlsUploadOnError) {
  base::test::ScopedFeatureList features{
      optimization_guide::features::kFormsClassificationsMqlsLogging};

  const FormData form;
  MockOnModelExecutedCallback on_model_executed;
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
  EXPECT_CALL(model_cache(),
              Update(CalculateFormSignature(form),
                     EqualsProto(AutofillAiTypeResponse()), IsEmpty()));
  EXPECT_CALL(on_model_executed, Run(form.global_id()));

  engine()->GetPredictions(form, on_model_executed.Get(), std::nullopt);

  EXPECT_THAT(mqls_uploader().uploaded_logs(), IsEmpty());
}

}  // namespace
}  // namespace autofill
