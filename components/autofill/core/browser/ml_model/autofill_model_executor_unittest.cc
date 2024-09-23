// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ModelExecutor =
    optimization_guide::ModelExecutor<AutofillModelEncoder::ModelOutput,
                                      const AutofillModelEncoder::ModelInput&>;
using TokenId = AutofillModelEncoder::TokenId;

class AutofillModelExecutorTest : public testing::Test {
 public:
  AutofillModelExecutorTest() = default;
  ~AutofillModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("autofill")
                           .AppendASCII("ml_model")
                           .AppendASCII("autofill_model-fold-one.tflite");
    execution_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    model_executor_ = std::make_unique<AutofillModelExecutor>();
    model_executor_->InitializeAndMoveToExecutionThread(
        /*model_inference_timeout=*/std::nullopt,
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
  base::test::ScopedFeatureList features_{features::kAutofillModelPredictions};
  base::FilePath model_file_path_;
  std::unique_ptr<AutofillModelExecutor> model_executor_;
};

TEST_F(AutofillModelExecutorTest, ExecuteModel) {
  // Update model file.
  execution_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelExecutor::UpdateModelFile,
                                model_executor_->GetWeakPtrForExecutionThread(),
                                model_file_path_));

  // Execute model on a dummy "form" consisting of two fields. Since the
  // executor works in terms of tokenized fields, this is represented as a
  // two arrays of tokens. The TokenIds are completely arbitrary.
  AutofillModelEncoder::ModelInput input = {
      {TokenId(1795), TokenId(1), TokenId(2), TokenId(3), TokenId(4),
       TokenId(5), TokenId(1797), TokenId(1), TokenId(2), TokenId(3),
       TokenId(4), TokenId(5), TokenId(1797), TokenId(1), TokenId(2),
       TokenId(3), TokenId(4), TokenId(5)},
      {TokenId(1795), TokenId(2), TokenId(3), TokenId(4), TokenId(5),
       TokenId(6), TokenId(1797), TokenId(2), TokenId(3), TokenId(4),
       TokenId(5), TokenId(6), TokenId(1796), TokenId(2), TokenId(3),
       TokenId(4), TokenId(5), TokenId(6)}};
  base::test::TestFuture<
      const std::optional<AutofillModelEncoder::ModelOutput>&>
      predictions;
  execution_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelExecutor::SendForExecution,
                                model_executor_->GetWeakPtrForExecutionThread(),
                                predictions.GetCallback(),
                                /*start_time=*/base::TimeTicks::Now(), input));

  // Expect that the execution succeeded. Since the input values are
  // meaningless, the meaning of the output is not validated. This is done in
  // the model handler tests, which actually have a type.
  ASSERT_TRUE(predictions.Get());
}

}  // namespace

}  // namespace autofill
