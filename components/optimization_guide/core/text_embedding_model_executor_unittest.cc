// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/text_embedding_model_handler.h"

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/buildflag.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class TextEmbeddingModelExecutorTest : public testing::Test {
 public:
  TextEmbeddingModelExecutorTest() {
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
    model_handler_ = std::make_unique<TextEmbeddingModelHandler>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(),
        /*model_metadata=*/absl::nullopt);
  }

  void PushModelFileToModelExecutor() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                         .AppendASCII("test")
                                         .AppendASCII("data")
                                         .AppendASCII("optimization_guide");
    // This is a dummy model. It won't apply for text embedding.
    model_file_path = model_file_path.AppendASCII("simple_test.tflite");
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder().SetModelFilePath(model_file_path).Build();
    model_handler_->OnModelUpdated(proto::OPTIMIZATION_TARGET_TEXT_EMBEDDER,
                                   *model_info);
    task_environment_.RunUntilIdle();
  }

  TextEmbeddingModelHandler* model_handler() { return model_handler_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<TextEmbeddingModelHandler> model_handler_;
};

TEST_F(TextEmbeddingModelExecutorTest, InvalidTextEmbeddingModel) {
  CreateModelHandler();

  PushModelFileToModelExecutor();
  EXPECT_TRUE(model_handler()->ModelAvailable());

  std::string input = "some text";
  base::RunLoop run_loop;

  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<tflite::task::processor::EmbeddingResult>&
                 output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          &run_loop),
      input);
  run_loop.Run();
}

}  // namespace optimization_guide
