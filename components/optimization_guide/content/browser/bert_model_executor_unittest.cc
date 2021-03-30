// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/bert_model_executor.h"

#include "base/path_service.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class BertModelExecutorTest : public testing::Test {
 public:
  void SetUp() override {
    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();
  }

  void CreateModelExecutor() {
    if (model_executor_)
      model_executor_.reset();

    model_executor_ = std::make_unique<BertModelExecutor>(
        optimization_guide_decider_.get(),
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
        /*model_metadata=*/base::nullopt,
        task_environment_.GetMainThreadTaskRunner());
  }

  void ResetModelExecutor() { model_executor_.reset(); }

  void PushModelFileToModelExecutor(bool is_valid) {
    DCHECK(model_executor_);

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                         .AppendASCII("test")
                                         .AppendASCII("data")
                                         .AppendASCII("optimization_guide");
    model_file_path =
        is_valid ? model_file_path.AppendASCII("bert_page_topics_model.tflite")
                 : model_file_path.AppendASCII("simple_test.tflite");
    model_executor_->OnModelFileUpdated(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt,
        model_file_path);
    task_environment_.RunUntilIdle();
  }

  BertModelExecutor* model_executor() { return model_executor_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<BertModelExecutor> model_executor_;
};

TEST_F(BertModelExecutorTest, ValidBertModel) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(/*is_valid=*/true);
  EXPECT_TRUE(model_executor()->HasLoadedModel());

  std::string input = "some text";
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const base::Optional<std::vector<tflite::task::core::Category>>&
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
  EXPECT_FALSE(model_executor()->HasLoadedModel());

  ResetModelExecutor();
}

}  // namespace optimization_guide
