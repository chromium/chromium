// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/bert_model_executor.h"

#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class BertModelExecutorTest : public testing::Test {
 public:
  void SetUp() override {
    optimization_guide_model_provider_ =
        std::make_unique<TestOptimizationGuideModelProvider>();
  }

  void TearDown() override {
    model_executor_handle_.reset();
    task_environment_.RunUntilIdle();
  }

  void CreateModelExecutor() {
    model_executor_handle_ = std::make_unique<BertModelExecutorHandle>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(),
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
        /*model_metadata=*/absl::nullopt);
  }

  void PushModelFileToModelExecutor(bool is_valid) {
    DCHECK(model_executor_handle_);

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                         .AppendASCII("test")
                                         .AppendASCII("data")
                                         .AppendASCII("optimization_guide");
    model_file_path =
        is_valid ? model_file_path.AppendASCII("bert_page_topics_model.tflite")
                 : model_file_path.AppendASCII("simple_test.tflite");
    model_executor_handle_->OnModelFileUpdated(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt,
        model_file_path);
    task_environment_.RunUntilIdle();
  }

  BertModelExecutorHandle* model_executor_handle() {
    return model_executor_handle_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<BertModelExecutorHandle> model_executor_handle_;
};

TEST_F(BertModelExecutorTest, ValidBertModel) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(/*is_valid=*/true);
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  std::string input = "some text";
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<tflite::task::core::Category>>&
                 output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();
}

TEST_F(BertModelExecutorTest, InvalidBertModel) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(/*is_valid=*/false);
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  std::string input = "some text";
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<tflite::task::core::Category>>&
                 output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();
}

}  // namespace optimization_guide
