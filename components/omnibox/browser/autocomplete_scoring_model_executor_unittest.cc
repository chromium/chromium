// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"

#include <memory>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ModelInput = AutocompleteScoringModelExecutor::ModelInput;
using ModelOutput = AutocompleteScoringModelExecutor::ModelOutput;

class AutocompleteScoringModelExecutorTest : public testing::Test {
 public:
  AutocompleteScoringModelExecutorTest() = default;
  ~AutocompleteScoringModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    // A model of `add` operator.
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("omnibox")
                           .AppendASCII("adder.tflite");
    execution_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    model_executor_ = std::make_unique<AutocompleteScoringModelExecutor>();
    model_executor_->InitializeAndMoveToExecutionThread(
        /*model_inference_timeout=*/std::nullopt,
        optimization_guide::proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING,
        execution_task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    // Destroy model executor.
    execution_task_runner_->DeleteSoon(FROM_HERE, std::move(model_executor_));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::FilePath model_file_path_;
  scoped_refptr<base::SequencedTaskRunner> execution_task_runner_;
  std::unique_ptr<AutocompleteScoringModelExecutor> model_executor_;
};

TEST_F(AutocompleteScoringModelExecutorTest, ExecuteModel) {
  // Update model file.
  execution_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &optimization_guide::ModelExecutor<ModelOutput,
                                             ModelInput>::UpdateModelFile,
          model_executor_->GetWeakPtrForExecutionThread(), model_file_path_));

  // Execute model.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  base::OnceCallback<void(const std::optional<ModelOutput>&)>
      execution_callback = base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::optional<ModelOutput>& output) {
            ASSERT_EQ(1, static_cast<int>(output.value().size()));
            // 1 + 2 = 3
            EXPECT_NEAR(3, output.value().front(), 1e-1);
            run_loop->Quit();
          },
          run_loop.get());
  base::TimeTicks now = base::TimeTicks::Now();
  ModelInput input = {1, 2};
  execution_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&optimization_guide::ModelExecutor<
                                    ModelOutput, ModelInput>::SendForExecution,
                                model_executor_->GetWeakPtrForExecutionThread(),
                                std::move(execution_callback), now, input));
  run_loop->Run();
}
