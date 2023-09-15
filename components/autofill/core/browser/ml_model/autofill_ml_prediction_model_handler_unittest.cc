// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"

#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// The matcher expects two arguments of types std::unique_ptr<AutofillField>
// and ServerFieldType respectively.
MATCHER(MlTypeEq, "") {
  return std::get<0>(arg)->heuristic_type(HeuristicSource::kMachineLearning) ==
         std::get<1>(arg);
}

class AutofillMlPredictionModelHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath test_data_dir = source_root_dir.AppendASCII("components")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("autofill")
                                       .AppendASCII("ml_model");
    base::FilePath model_file_path =
        test_data_dir.AppendASCII("autofill_model-br-overfit.tflite");
    base::FilePath dictionary_path =
        test_data_dir.AppendASCII("br_overfitted_dictionary_test.txt");
    features_.InitAndEnableFeatureWithParameters(
        features::kAutofillModelPredictions,
        {{features::kAutofillModelDictionaryFilePath.name,
          dictionary_path.MaybeAsASCII()}});
    model_handler_ = std::make_unique<AutofillMlPredictionModelHandler>(
        model_provider_.get());
    SimulateRetrieveModelFromServer(model_file_path);
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    model_handler_.reset();
    task_environment_.RunUntilIdle();
  }

  void SimulateRetrieveModelFromServer(const base::FilePath& model_file_path) {
    auto model_info = optimization_guide::TestModelInfoBuilder()
                          .SetModelFilePath(model_file_path)
                          .Build();
    model_handler_->OnModelUpdated(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
        *model_info);
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<AutofillMlPredictionModelHandler> model_handler_;
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_environment_;
};

}  // namespace

TEST_F(AutofillMlPredictionModelHandlerTest, ModelExecutedFormData) {
  auto form_structure = std::make_unique<FormStructure>(
      test::GetFormData({.fields = {{.label = u"nome completo"},
                                    {.label = u"cpf"},
                                    {.label = u"data de nascimento ddmmaaaa"},
                                    {.label = u"seu telefone"},
                                    {.label = u"email"},
                                    {.label = u"senha"},
                                    {.label = u"cep"}}}));
  base::test::TestFuture<std::unique_ptr<FormStructure>> future;
  model_handler_->GetModelPredictionsForForm(std::move(form_structure),
                                             future.GetCallback());
  EXPECT_THAT(
      future.Get()->fields(),
      testing::Pointwise(MlTypeEq(), {NAME_FULL, UNKNOWN_TYPE, UNKNOWN_TYPE,
                                      PHONE_HOME_CITY_AND_NUMBER, EMAIL_ADDRESS,
                                      UNKNOWN_TYPE, ADDRESS_HOME_ZIP}));
}

}  // namespace autofill
