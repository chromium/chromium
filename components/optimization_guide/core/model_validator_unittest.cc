// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_validator.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class ModelValidatorExecutorTest : public testing::Test {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kModelValidate);
    optimization_guide_model_provider_ =
        std::make_unique<TestOptimizationGuideModelProvider>();
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  void ValidateModel(const base::FilePath& model_file_path) {
    DoValidateModel(optimization_guide_model_provider_.get());
    optimization_guide_model_provider_->NotifyModelFileUpdate(
        proto::OPTIMIZATION_TARGET_MODEL_VALIDATION, model_file_path);
    task_environment_.RunUntilIdle();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
};

TEST_F(ModelValidatorExecutorTest, ValidModel) {
  base::FilePath model_file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &model_file_path);
  model_file_path = model_file_path.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("optimization_guide")
                        .AppendASCII("simple_test.tflite");
  ValidateModel(model_file_path);

  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadingResult." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      optimization_guide::ModelExecutorLoadingState::
          kModelFileValidAndMemoryMapped,
      1);
  histogram_tester().ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ModelLoadingDuration." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      1);
}

TEST_F(ModelValidatorExecutorTest, InvalidModel) {
  base::FilePath invalid_model_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&invalid_model_file_path));
  base::WriteFile(invalid_model_file_path, "INVALID MODEL DATA");
  ValidateModel(invalid_model_file_path);

  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadingResult." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      optimization_guide::ModelExecutorLoadingState::kModelFileInvalid, 1);
  histogram_tester().ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ModelLoadingDuration." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      1);
}

}  // namespace optimization_guide