// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"

#include <optional>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::optimization_guide::AnyWrapProto;
using optimization_guide::proto::AutofillFieldClassificationModelMetadata;
using optimization_guide::proto::
    AutofillFieldClassificationPostprocessingParameters;

using MockExecuteModelCallback = base::OnceCallback<void(
    const std::optional<FieldClassificationModelEncoder::ModelOutput>&)>;

// The matcher expects two arguments of types std::unique_ptr<AutofillField>
// and FieldType respectively. It accesses `local_type_predictions_` directly
// because heuristic_type() returns the post-processed prediction, after
// potentially falling back to regex.
MATCHER(MlTypeEq, "") {
  return std::get<0>(arg)->local_type_predictions()[static_cast<size_t>(
             HeuristicSource::kAutofillMachineLearning)] == std::get<1>(arg);
}

FieldClassificationModelEncoder::ModelOutput
CreateMockExecutorOutputForOverfittedForm() {
  return {{0.0f, 0.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
          {1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
          {0.0f, 0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
          {0.0f, 1.0f, 0.0f, 0.0f, 0.0f}};
}

// Mock class for FieldClassificationModelHandler to intercept
// ExecuteModelWithInput.
class MockFieldClassificationModelHandler
    : public FieldClassificationModelHandler {
 public:
  MockFieldClassificationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target)
      : FieldClassificationModelHandler(model_provider, optimization_target) {}

  MOCK_METHOD(
      void,
      ExecuteModelWithInput,
      (base::OnceCallback<void(
           const std::optional<FieldClassificationModelEncoder::ModelOutput>&)>
           callback,
       const FieldClassificationModelEncoder::ModelInput& input),
      (override));
};

class FieldClassificationModelHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    test_data_dir_ = source_root_dir.AppendASCII("components")
                         .AppendASCII("test")
                         .AppendASCII("data")
                         .AppendASCII("autofill")
                         .AppendASCII("ml_model");
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
  }

  optimization_guide::TestOptimizationGuideModelProvider& model_provider() {
    return *model_provider_;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  AutofillFieldClassificationModelMetadata& model_metadata() {
    return model_metadata_;
  }

  virtual FieldClassificationModelHandler& model_handler() = 0;

  void ApplyModelPredictions(std::unique_ptr<FormStructure>& form_structure) {
    base::test::TestFuture<ModelPredictions> future;
    model_handler().GetModelPredictionsForForm(form_structure->ToFormData(),
                                               GeoIpCountryCode("US"),
                                               future.GetCallback());
    future.Get().ApplyTo(form_structure->fields());
  }

  void ApplyModelPredictions(
      base::span<const std::unique_ptr<FormStructure>> form_structures) {
    base::test::TestFuture<std::vector<ModelPredictions>> future;
    model_handler().GetModelPredictionsForForms(
        base::ToVector(form_structures,
                       [](const auto& form_structure) {
                         return form_structure->ToFormData();
                       }),
        GeoIpCountryCode("US"), future.GetCallback());
    for (auto [form_structure, predictions] :
         base::zip(form_structures, future.Get())) {
      predictions.ApplyTo(form_structure->fields());
    }
  }

  // The overfitted model is overtrained on this form. Which is the only form
  // that can be used for unittests. The model that is
  // provided by the server side is trained on many different other forms.
  std::unique_ptr<FormStructure> CreateOverfittedForm() const {
    return std::make_unique<FormStructure>(
        test::GetFormData({.fields = {{.label = u"nome completo"},
                                      {.label = u"cpf"},
                                      {.label = u"data de nascimento ddmmaaaa"},
                                      {.label = u"seu telefone"},
                                      {.label = u"email"},
                                      {.label = u"senha"},
                                      {.label = u"cep"}}}));
  }

  // The expected types for the form in `CreateOverfittedForm()` using the
  // overfitted model.
  std::vector<FieldType> ExpectedTypesForOverfittedForm() const {
    return {NAME_FULL,       UNKNOWN_TYPE,
            UNKNOWN_TYPE,    PHONE_HOME_CITY_AND_NUMBER,
            EMAIL_ADDRESS,   UNKNOWN_TYPE,
            ADDRESS_HOME_ZIP};
  }

  // Simulates receiving the model from the server, with `model_metadata_`
  // attached.
  void SimulateRetrieveModelFromServer(
      const std::string file_name,
      FieldClassificationModelHandler& model_handler) {
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(test_data_dir_.AppendASCII(file_name))
            .SetModelMetadata(AnyWrapProto(model_metadata_))
            .Build();
    model_handler.OnModelUpdated(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
        *model_info);
    task_environment_.RunUntilIdle();
  }

  void ReadModelMetadata(const std::string file_name) {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        metadata;
    base::FilePath file_path(test_data_dir_.AppendASCII(file_name));
    std::string proto_content;
    EXPECT_TRUE(base::ReadFileToString(file_path, &proto_content));
    EXPECT_TRUE(metadata.ParseFromString(proto_content));
    model_metadata_ = metadata;
  }

 private:
  // Populates `metadata.input_token()` with the contents of the file located
  // at `dictionary_path`. Each line of the dictionary file is added as a
  // separate token.
  void AddInputTokensFromFile(
      const base::FilePath& dictionary_path,
      optimization_guide::proto::AutofillFieldClassificationModelMetadata&
          metadata) const {
    std::string dictionary_content;
    EXPECT_TRUE(base::ReadFileToString(dictionary_path, &dictionary_content));
    for (const std::string& token :
         base::SplitString(dictionary_content, "\n", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)) {
      metadata.add_input_token(token);
    }
  }

  base::test::ScopedFeatureList features_{
      autofill::features::kAutofillModelPredictions};
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_environment_;
  base::FilePath test_data_dir_;
  AutofillFieldClassificationModelMetadata model_metadata_;
};

class FieldClassificationModelHandlerTestWithRealModelExecution
    : public FieldClassificationModelHandlerTest {
 public:
  void SetUp() override {
    FieldClassificationModelHandlerTest::SetUp();
    model_handler_ = std::make_unique<FieldClassificationModelHandler>(
        &model_provider(),
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION);
    task_environment().RunUntilIdle();
  }

  void TearDown() override {
    model_handler_.reset();
    task_environment().RunUntilIdle();
  }

  FieldClassificationModelHandler& model_handler() override {
    return *model_handler_;
  }

 private:
  std::unique_ptr<FieldClassificationModelHandler> model_handler_;
};

// Test that supported types are registered correctly when the model is loaded.
TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       SupportedTypesSetCorrectlyOnModelUpdate) {
  ReadModelMetadata("autofill_model_metadata.binarypb");
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());
  FieldTypeSet supported_types = model_handler().get_supported_types();
  ASSERT_TRUE(supported_types.contains(FieldType::ADDRESS_HOME_ZIP));
  ASSERT_FALSE(supported_types.contains(FieldType::IBAN_VALUE));
}

TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       GetModelPredictionsForForm) {
  ReadModelMetadata("autofill_model_metadata.binarypb");
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());
  std::unique_ptr<FormStructure> form_structure = CreateOverfittedForm();
  ApplyModelPredictions(form_structure);
  EXPECT_THAT(form_structure->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
}

// Tests that predictions with a confidence below the threshold are reported as
// NO_SERVER_DATA - with small form rules disabled.
TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       GetModelPredictionsForForm_Threshold_WithoutSmallFormRules) {
  // Disable small form rules.
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kAutofillModelPredictions,
      {{features::kAutofillModelPredictionsSmallFormRules.name, "false"}});

  // Set a really high threshold and expect that all predictions are suppressed.
  ReadModelMetadata("autofill_model_metadata.binarypb");
  model_metadata()
      .mutable_postprocessing_parameters()
      ->set_confidence_threshold_per_field(/*confidence_threshold=*/100);
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());
  std::unique_ptr<FormStructure> form_structure = CreateOverfittedForm();
  ApplyModelPredictions(form_structure);
  EXPECT_THAT(form_structure->fields(),
              testing::Pointwise(MlTypeEq(), std::vector<FieldType>(
                                                 form_structure->field_count(),
                                                 NO_SERVER_DATA)));
}

// Tests that predictions with a confidence below the threshold are reported as
// NO_SERVER_DATA - with small form rules enabled.
TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       GetModelPredictionsForForm_Threshold_WithSmallFormRules) {
  // Enable small form rules.
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kAutofillModelPredictions,
      {{features::kAutofillModelPredictionsSmallFormRules.name, "true"}});

  // Set a really high threshold and expect that all predictions are suppressed.
  ReadModelMetadata("autofill_model_metadata.binarypb");
  model_metadata()
      .mutable_postprocessing_parameters()
      ->set_confidence_threshold_per_field(/*confidence_threshold=*/100);
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());
  std::unique_ptr<FormStructure> form_structure = CreateOverfittedForm();
  ApplyModelPredictions(form_structure);
  EXPECT_THAT(form_structure->fields(),
              testing::Pointwise(MlTypeEq(), std::vector<FieldType>(
                                                 form_structure->field_count(),
                                                 NO_SERVER_DATA)));
}

TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       GetModelPredictionsForForms) {
  ReadModelMetadata("autofill_model_metadata.binarypb");
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());
  std::vector<std::unique_ptr<FormStructure>> forms;
  forms.push_back(CreateOverfittedForm());
  forms.push_back(CreateOverfittedForm());
  ApplyModelPredictions(forms);
  EXPECT_THAT(forms[0]->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
  EXPECT_THAT(forms[1]->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
}

// Tests that if the form metadata allows returning empty predictions, and model
// outputs do not reach the desired confidence level, empty predictions will be
// emitted.
TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       GetModelPredictionsForForm_NoPredictionsEmitted) {
  // Set min required confidence to be very high, even for the overfitted model.
  ReadModelMetadata("autofill_model_metadata.binarypb");
  model_metadata()
      .mutable_postprocessing_parameters()
      ->set_confidence_threshold_to_disable_all_predictions(0.999);
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());

  std::unique_ptr<FormStructure> form_structure = CreateOverfittedForm();
  ApplyModelPredictions(form_structure);

  // `NO_SERVER_DATA` means the type could not be set.
  EXPECT_THAT(form_structure->fields(),
              testing::Pointwise(MlTypeEq(), std::vector<FieldType>(
                                                 form_structure->field_count(),
                                                 NO_SERVER_DATA)));
}

// Tests that if the form metadata allows returning empty predictions, non-empty
// predictions will still be emitted for predictions with high confidence.
TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       GetModelPredictionsForForm_PredictionsEmittedWithMinConfidence) {
  ReadModelMetadata("autofill_model_metadata.binarypb");
  model_metadata()
      .mutable_postprocessing_parameters()
      ->set_confidence_threshold_to_disable_all_predictions(0.5);
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());

  std::unique_ptr<FormStructure> form_structure = CreateOverfittedForm();
  ApplyModelPredictions(form_structure);

  // An overfitted model is very confident in its predictions, so non-empty
  // predictions should be emitted.
  EXPECT_THAT(form_structure->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
}

// Tests that if the form metadata does not allow assigning the same type to
// multiple fields, NO_SERVER_DATA type will be assigned to a field with less
// confidence.
TEST_F(FieldClassificationModelHandlerTestWithRealModelExecution,
       GetModelPredictionsForForm_DisallowSameTypePredictions) {
  ReadModelMetadata("model_with_repeated_predicted_types.binarypb");
  SimulateRetrieveModelFromServer("model_with_repeated_predicted_types.tflite",
                                  model_handler());

  std::unique_ptr<FormStructure> overfitted_form =
      std::make_unique<FormStructure>(
          test::GetFormData({.fields = {{.label = u"username"},
                                        {.label = u"repeat username"},
                                        {.label = u"new password"},
                                        {.label = u"confirm password"}}}));

  ApplyModelPredictions(overfitted_form);

  // The model is trained to predict USERNAME on the first two fields. Expect
  // that the second field prediction will be discarded and replaced with
  // NO_SERVER_DATA.
  auto expected_predictions = {USERNAME, NO_SERVER_DATA,
                               ACCOUNT_CREATION_PASSWORD,
                               CONFIRMATION_PASSWORD};
  EXPECT_THAT(overfitted_form->fields(),
              testing::Pointwise(MlTypeEq(), expected_predictions));
}

class FieldClassificationModelHandlerTestWithMockedModelExecution
    : public FieldClassificationModelHandlerTest {
 public:
  void SetUp() override {
    FieldClassificationModelHandlerTest::SetUp();
    // Create a mock model handler to allow mocking the actual model execution.
    mocked_execution_handler_ =
        std::make_unique<MockFieldClassificationModelHandler>(
            &model_provider(),
            optimization_guide::proto::OptimizationTarget::
                OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION);
    ReadModelMetadata("autofill_model_metadata.binarypb");
    SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                    model_handler());
  }

 protected:
  MockFieldClassificationModelHandler& model_handler() override {
    return *mocked_execution_handler_;
  }

 private:
  base::test::ScopedFeatureList feature_list{
      features::kFieldClassificationModelCaching};
  std::unique_ptr<MockFieldClassificationModelHandler>
      mocked_execution_handler_;
};

TEST_F(FieldClassificationModelHandlerTestWithMockedModelExecution,
       CacheHitAndMiss) {
  EXPECT_CALL(model_handler(), ExecuteModelWithInput)
      .WillOnce([](MockExecuteModelCallback callback,
                   const FieldClassificationModelEncoder::ModelInput& input) {
        std::move(callback).Run(CreateMockExecutorOutputForOverfittedForm());
      });
  std::unique_ptr<FormStructure> result1 = CreateOverfittedForm();
  ApplyModelPredictions(result1);
  // Ensure the model output was applied.
  EXPECT_THAT(result1->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));

  // Second call should use the cached result and not execute the model again.
  EXPECT_CALL(model_handler(), ExecuteModelWithInput).Times(0);
  std::unique_ptr<FormStructure> result2 = CreateOverfittedForm();
  ApplyModelPredictions(result2);
  // Check that the cached results are used.
  EXPECT_THAT(result2->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));

  // Query predictions for a different form. Verify that the model is run again.
  std::unique_ptr<FormStructure> different_form =
      std::make_unique<FormStructure>(
          test::GetFormData({.fields = {{.label = u"different label"},
                                        {.label = u"another field"}}}));
  FieldClassificationModelEncoder::ModelOutput mock_output_for_different_form =
      {{0.0f, 0.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
  EXPECT_CALL(model_handler(), ExecuteModelWithInput)
      .WillOnce([mock_output_for_different_form](
                    MockExecuteModelCallback callback,
                    const FieldClassificationModelEncoder::ModelInput& input) {
        std::move(callback).Run(mock_output_for_different_form);
      });
  ApplyModelPredictions(different_form);
}

TEST_F(FieldClassificationModelHandlerTestWithMockedModelExecution,
       CacheInvalidationOnModelUpdate) {
  EXPECT_CALL(model_handler(), ExecuteModelWithInput)
      .WillOnce([](MockExecuteModelCallback callback,
                   const FieldClassificationModelEncoder::ModelInput& input) {
        std::move(callback).Run(CreateMockExecutorOutputForOverfittedForm());
      });
  std::unique_ptr<FormStructure> result1 = CreateOverfittedForm();
  ApplyModelPredictions(result1);
  EXPECT_THAT(result1->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));

  // Simulate a model update. This should clear the cache.
  SimulateRetrieveModelFromServer("autofill_model-fold-one.tflite",
                                  model_handler());

  // Query predictions for the same form again and check that the model is
  // executed again.
  EXPECT_CALL(model_handler(), ExecuteModelWithInput)
      .WillOnce([](MockExecuteModelCallback callback,
                   const FieldClassificationModelEncoder::ModelInput& input) {
        std::move(callback).Run(CreateMockExecutorOutputForOverfittedForm());
      });
  std::unique_ptr<FormStructure> result2 = CreateOverfittedForm();
  ApplyModelPredictions(result2);
  EXPECT_THAT(result2->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
}

// Sanity check that predictions for a small form are not cleared if the small
// form rules are not enabled.
TEST_F(FieldClassificationModelHandlerTestWithMockedModelExecution,
       SmallFormRulesDisabled_PredictionsNotCleared) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kAutofillModelPredictions,
      {{features::kAutofillModelPredictionsSmallFormRules.name, "false"}});
  ASSERT_FALSE(model_handler().ShouldApplySmallFormRules());
  auto small_form = std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.label = u"Name"}}}));

  EXPECT_CALL(model_handler(), ExecuteModelWithInput)
      .WillOnce([](MockExecuteModelCallback callback,
                   const FieldClassificationModelEncoder::ModelInput& input) {
        // Mock a NAME_FULL prediction.
        std::move(callback).Run(FieldClassificationModelEncoder::ModelOutput{
            {0.0, 0.0, 1.0, 0.0, 0.0}});
      });
  ApplyModelPredictions(small_form);

  // The heuristic type should be what the model predicted, since the small form
  // rules are disabled.
  EXPECT_THAT(small_form->fields(),
              testing::Pointwise(MlTypeEq(), {NAME_FULL}));
}

// Test that predictions are cleared for small forms when the small form rules
// are enabled via feature params.
TEST_F(FieldClassificationModelHandlerTestWithMockedModelExecution,
       SmallFormRulesEnabled_PredictionsCleared) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      features::kAutofillModelPredictions,
      {{features::kAutofillModelPredictionsSmallFormRules.name, "true"}});
  ASSERT_TRUE(model_handler().ShouldApplySmallFormRules());

  auto small_form = std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.label = u"Nome completo"}}}));

  EXPECT_CALL(model_handler(), ExecuteModelWithInput)
      .WillOnce([](MockExecuteModelCallback callback,
                   const FieldClassificationModelEncoder::ModelInput& input) {
        // Mock a NAME_FULL prediction.
        std::move(callback).Run(FieldClassificationModelEncoder::ModelOutput{
            {0.0, 0.0, 1.0, 0.0, 0.0}});
      });
  ApplyModelPredictions(small_form);

  // The small form rules should have cleared the prediction.
  EXPECT_THAT(small_form->fields(),
              testing::Pointwise(MlTypeEq(), {UNKNOWN_TYPE}));
}

}  // namespace
}  // namespace autofill
