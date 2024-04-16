// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/bert_model_handler.h"

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/buildflag.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class BertModelExecutorTest : public testing::Test {
 public:
  BertModelExecutorTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kPreventLongRunningPredictionModels);
  }

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
        /*model_metadata=*/std::nullopt);
  }

  void PushModelFileToModelExecutor(bool is_valid) {
    DCHECK(model_handler_);

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
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
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<BertModelHandler> model_handler_;
};

// TODO(crbug.com/40848529): Running the model is slow and times out tests on
// many platforms. Ideally, we can schedule this to run infrequently but for
// now we will only load the model.
TEST_F(BertModelExecutorTest, ValidBertModel) {
  CreateModelHandler();

  PushModelFileToModelExecutor(/*is_valid=*/true);
  EXPECT_TRUE(model_handler()->ModelAvailable());
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
             const std::optional<std::vector<tflite::task::core::Category>>&
                 output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();
}

}  // namespace optimization_guide
