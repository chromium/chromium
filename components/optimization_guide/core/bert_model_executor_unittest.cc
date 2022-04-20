// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/bert_model_handler.h"

#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
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
    model_handler_.reset();
    task_environment_.RunUntilIdle();
  }

  void CreateModelHandler() {
    model_handler_ = std::make_unique<BertModelHandler>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(),
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
        /*model_metadata=*/absl::nullopt);
  }

  void PushModelFileToModelExecutor(bool is_valid) {
    DCHECK(model_handler_);

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                         .AppendASCII("test")
                                         .AppendASCII("data")
                                         .AppendASCII("optimization_guide");
    model_file_path =
        is_valid ? model_file_path.AppendASCII("bert_page_topics_model.tflite")
                 : model_file_path.AppendASCII("simple_test.tflite");
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder().SetModelFilePath(model_file_path).Build();
    model_handler_->OnModelUpdated(proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
                                   *model_info);
    task_environment_.RunUntilIdle();
  }

  BertModelHandler* model_handler() { return model_handler_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<BertModelHandler> model_handler_;
};

TEST_F(BertModelExecutorTest, ValidBertModel) {
  CreateModelHandler();

  PushModelFileToModelExecutor(/*is_valid=*/true);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  std::string input = "some text";
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
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
  CreateModelHandler();

  PushModelFileToModelExecutor(/*is_valid=*/false);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  std::string input = "some text";
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
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
