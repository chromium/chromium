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
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using testing::ElementsAre;

namespace {

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
        test_data_dir.AppendASCII("autofill_model_baseline.tflite");
    base::FilePath dictionary_path =
        test_data_dir.AppendASCII("dictionary_test.txt");
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

  std::vector<ServerFieldType> GetModelPredictions(const FormData& form_data) {
    base::test::TestFuture<const std::vector<ServerFieldType>&> future;
    model_handler_->GetModelPredictionsForForm(form_data, future.GetCallback());
    return future.Get();
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
  FormData form_data = test::GetFormData({.fields = {
                                              {.label = u"First name"},
                                              {.label = u"Last name"},
                                              {.label = u"Address line 1"},
                                          }});
  EXPECT_THAT(GetModelPredictions(form_data),
              ElementsAre(NAME_FIRST, NAME_LAST, ADDRESS_HOME_LINE1));
}

TEST_F(AutofillMlPredictionModelHandlerTest, ModelExecutedMultipleForms) {
  {
    FormData form_data = test::GetFormData({.fields = {
                                                {.label = u"First name"},
                                                {.label = u"Last name"},
                                            }});
    EXPECT_THAT(GetModelPredictions(form_data),
                ElementsAre(NAME_FIRST, NAME_LAST));
  }
  {
    FormData form_data =
        test::GetFormData({.fields = {
                               {.label = u"Credit card name"},
                               {.label = u"Credit card number"},
                           }});
    EXPECT_THAT(GetModelPredictions(form_data),
                ElementsAre(CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER));
  }
}

TEST_F(AutofillMlPredictionModelHandlerTest, ModelExecutedEmptyForm) {
  FormData form_data;
  EXPECT_TRUE(GetModelPredictions(form_data).empty());
}

}  // namespace autofill
