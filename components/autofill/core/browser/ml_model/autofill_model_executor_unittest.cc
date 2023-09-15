// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"

#include "base/base_paths.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillModelExecutorTest : public testing::Test {
 public:
  AutofillModelExecutorTest() = default;
  ~AutofillModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath test_data_dir = source_root_dir.AppendASCII("components")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("autofill")
                                       .AppendASCII("ml_model");
    model_file_path_ =
        test_data_dir.AppendASCII("autofill_model-br-overfit.tflite");
    base::FilePath dictionary_path =
        test_data_dir.AppendASCII("br_overfitted_dictionary_test.txt");
    features_.InitAndEnableFeatureWithParameters(
        features::kAutofillModelPredictions,
        {{features::kAutofillModelDictionaryFilePath.name,
          dictionary_path.MaybeAsASCII()}});
    execution_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    model_executor_ = std::make_unique<AutofillModelExecutor>();
    model_executor_->InitializeAndMoveToExecutionThread(
        /*model_inference_timeout=*/absl::nullopt,
        optimization_guide::proto::
            OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
        execution_task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    execution_task_runner_->DeleteSoon(FROM_HERE, std::move(model_executor_));
    task_environment_.RunUntilIdle();
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_environment_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> execution_task_runner_;
  base::FilePath model_file_path_;
  base::test::ScopedFeatureList features_;
  std::unique_ptr<AutofillModelExecutor> model_executor_;
};

TEST_F(AutofillModelExecutorTest, ExecuteModel) {
  // Update model file.
  execution_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &optimization_guide::ModelExecutor<std::vector<ServerFieldType>,
                                             const FormData&>::UpdateModelFile,
          model_executor_->GetWeakPtrForExecutionThread(), model_file_path_));
  // Execute model.
  base::RunLoop run_loop;
  absl::optional<std::vector<ServerFieldType>> predictions;
  base::OnceCallback<void(const absl::optional<std::vector<ServerFieldType>>&)>
      execution_callback = base::BindLambdaForTesting(
          [&](const absl::optional<std::vector<ServerFieldType>>& output) {
            predictions = output;
            run_loop.Quit();
          });
  // The overfitted model is trained on this exact form in this order so this
  // is the only form that can be used for unittests. The model that will be
  // provided by the server side will be trained on many different other forms.
  FormData form_data =
      test::GetFormData({.fields = {{.label = u"nome completo"},
                                    {.label = u"cpf"},
                                    {.label = u"data de nascimento ddmmaaaa"},
                                    {.label = u"seu telefone"},
                                    {.label = u"email"},
                                    {.label = u"senha"},
                                    {.label = u"cep"}}});
  execution_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &optimization_guide::ModelExecutor<std::vector<ServerFieldType>,
                                             const FormData&>::SendForExecution,
          model_executor_->GetWeakPtrForExecutionThread(),
          std::move(execution_callback),
          /*start_time=*/AutofillTickClock::NowTicks(), form_data));
  run_loop.Run();
  ASSERT_TRUE(predictions.has_value());
  EXPECT_THAT(predictions.value(),
              testing::ElementsAre(NAME_FULL, UNKNOWN_TYPE, UNKNOWN_TYPE,
                                   PHONE_HOME_CITY_AND_NUMBER, EMAIL_ADDRESS,
                                   UNKNOWN_TYPE, ADDRESS_HOME_ZIP));
}

}  // namespace autofill
