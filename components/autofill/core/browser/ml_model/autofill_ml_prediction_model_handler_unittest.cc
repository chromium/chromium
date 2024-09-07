// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"

#include <optional>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// The matcher expects two arguments of types std::unique_ptr<AutofillField>
// and FieldType respectively.
MATCHER(MlTypeEq, "") {
  return std::get<0>(arg)->heuristic_type(HeuristicSource::kMachineLearning) ==
         std::get<1>(arg);
}

class AutofillMlPredictionModelHandlerTest : public testing::Test {
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
    model_handler_ = std::make_unique<AutofillMlPredictionModelHandler>(
        model_provider_.get());
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    model_handler_.reset();
    task_environment_.RunUntilIdle();
  }

  AutofillMlPredictionModelHandler& model_handler() { return *model_handler_; }

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

  // Simulates receiving the model from the server, with metadata attached.
  // An optional `confidence_threshold` for the metadata can be provided.
  void SimulateRetrieveModelFromServer(
      std::optional<float> confidence_threshold = std::nullopt) {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        model_metadata = ReadModelMetadata();
    if (confidence_threshold) {
      model_metadata.set_confidence_threshold(*confidence_threshold);
    }
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(
                test_data_dir_.AppendASCII("autofill_model-fold-one.tflite"))
            .SetModelMetadata(WrapMetadata(model_metadata))
            .Build();
    model_handler_->OnModelUpdated(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
        *model_info);
    task_environment_.RunUntilIdle();
  }

 private:
  optimization_guide::proto::AutofillFieldClassificationModelMetadata
  ReadModelMetadata() const {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        metadata;
    base::FilePath file_path(
        test_data_dir_.AppendASCII("autofill_model_metadata.binarypb"));
    std::string proto_content;
    EXPECT_TRUE(base::ReadFileToString(file_path, &proto_content));
    EXPECT_TRUE(metadata.ParseFromString(proto_content));
    return metadata;
  }

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

  // Packs the `metadata` into an `optimization_guide::proto::Any`.
  optimization_guide::proto::Any WrapMetadata(
      const optimization_guide::proto::AutofillFieldClassificationModelMetadata&
          metadata) const {
    std::string serialized_metadata;
    metadata.SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any;
    any.set_value(serialized_metadata);
    any.set_type_url(
        "type.googleapis.com/"
        "optimization_guide.proto.AutofillFieldClassificationModelMetadata");
    return any;
  }

  base::test::ScopedFeatureList features_{features::kAutofillModelPredictions};
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<AutofillMlPredictionModelHandler> model_handler_;
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_environment_;
  base::FilePath test_data_dir_;
};

TEST_F(AutofillMlPredictionModelHandlerTest, GetModelPredictionsForForm) {
  SimulateRetrieveModelFromServer();
  std::unique_ptr<FormStructure> form_structure = CreateOverfittedForm();
  base::test::TestFuture<std::unique_ptr<FormStructure>> future;
  model_handler().GetModelPredictionsForForm(std::move(form_structure),
                                             future.GetCallback());
  EXPECT_THAT(future.Get()->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
}

// Tests that predictions with a confidence below the threshold are reported as
// UNKNOWN_TYPE.
TEST_F(AutofillMlPredictionModelHandlerTest,
       GetModelPredictionsForForm_Threshold) {
  // Set a really high threshold and expect that all predictions are suppressed.
  SimulateRetrieveModelFromServer(/*confidence_threshold=*/100);
  std::unique_ptr<FormStructure> form_structure = CreateOverfittedForm();
  base::test::TestFuture<std::unique_ptr<FormStructure>> future;
  model_handler().GetModelPredictionsForForm(std::move(form_structure),
                                             future.GetCallback());
  EXPECT_THAT(future.Get()->fields(),
              testing::Pointwise(
                  MlTypeEq(), std::vector<FieldType>(
                                  future.Get()->field_count(), UNKNOWN_TYPE)));
}

TEST_F(AutofillMlPredictionModelHandlerTest, GetModelPredictionsForForms) {
  SimulateRetrieveModelFromServer();
  std::vector<std::unique_ptr<FormStructure>> forms;
  forms.push_back(CreateOverfittedForm());
  forms.push_back(CreateOverfittedForm());
  base::test::TestFuture<std::vector<std::unique_ptr<FormStructure>>> future;
  model_handler().GetModelPredictionsForForms(std::move(forms),
                                              future.GetCallback());
  ASSERT_EQ(future.Get().size(), 2u);
  EXPECT_THAT(future.Get()[0]->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
  EXPECT_THAT(future.Get()[1]->fields(),
              testing::Pointwise(MlTypeEq(), ExpectedTypesForOverfittedForm()));
}

}  // namespace
}  // namespace autofill
