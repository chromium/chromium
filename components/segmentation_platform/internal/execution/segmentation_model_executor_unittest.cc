// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/segmentation_model_executor.h"

#include <memory>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const auto kOptimizationTarget = optimization_guide::proto::OptimizationTarget::
    OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
}  // namespace

namespace segmentation_platform {

class SegmentationModelExecutorTest : public testing::Test {
 public:
  SegmentationModelExecutorTest() = default;
  ~SegmentationModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("segmentation_platform")
                           .AppendASCII("adder.tflite");

    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
  }

  void TearDown() override { ResetModelExecutor(); }

  void CreateModelExecutor() {
    if (model_executor_handle_)
      model_executor_handle_.reset();

    model_executor_handle_ = std::make_unique<SegmentationModelHandler>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(), kOptimizationTarget);
  }

  void ResetModelExecutor() {
    model_executor_handle_.reset();
    // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
    // to be destroyed.
    RunUntilIdle();
  }

  void PushModelFileToModelExecutor() {
    DCHECK(model_executor_handle_);
    model_executor_handle_->OnModelFileUpdated(kOptimizationTarget,
                                               absl::nullopt, model_file_path_);
    RunUntilIdle();
  }

  SegmentationModelHandler* model_executor_handle() {
    return model_executor_handle_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  base::FilePath model_file_path_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;

  std::unique_ptr<SegmentationModelHandler> model_executor_handle_;
};

TEST_F(SegmentationModelExecutorTest, ExecuteWithLoadedModel) {
  CreateModelExecutor();

  PushModelFileToModelExecutor();
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  std::vector<float> input = {4, 5};

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop, const absl::optional<float>& output) {
            EXPECT_TRUE(output.has_value());
            // 4 + 5 = 9
            EXPECT_NEAR(9, output.value(), 1e-1);

            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  ResetModelExecutor();
}

}  // namespace segmentation_platform
