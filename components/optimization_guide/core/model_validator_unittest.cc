// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_validator.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

void DoValidateModel(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  DCHECK(switches::ShouldValidateModel());

  // Create the validator object which will get destroyed when the model load is
  // complete.
  new ModelValidatorHandler(
      optimization_guide_model_provider,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

}  // namespace

class ModelValidatorModelObserverTracker
    : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    if (optimization_target == proto::OPTIMIZATION_TARGET_MODEL_VALIDATION) {
      EXPECT_FALSE(model_validation_observer_);
      model_validation_observer_ = observer;
    }
  }

  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) override {
    if (optimization_target == proto::OPTIMIZATION_TARGET_MODEL_VALIDATION) {
      EXPECT_EQ(observer, model_validation_observer_);
      model_validation_observer_ = nullptr;
    }
  }

  // Notifies the model validation observer about the model file update.
  void NotifyModelFileUpdate(proto::OptimizationTarget optimization_target,
                             const base::FilePath& model_file_path) {
    if (optimization_target == proto::OPTIMIZATION_TARGET_MODEL_VALIDATION) {
      auto model_metadata =
          TestModelInfoBuilder().SetModelFilePath(model_file_path).Build();
      model_validation_observer_->OnModelUpdated(optimization_target,
                                                 *model_metadata);
    }
  }

 private:
  // The observer that is registered to receive model validation optimzation
  // target events.
  raw_ptr<OptimizationTargetModelObserver> model_validation_observer_;
};

class ModelValidatorExecutorTest : public testing::Test {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kModelValidate);
    model_observer_tracker_ =
        std::make_unique<ModelValidatorModelObserverTracker>();
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  void ValidateModel(const base::FilePath& model_file_path) {
    DoValidateModel(model_observer_tracker_.get());
    model_observer_tracker_->NotifyModelFileUpdate(
        proto::OPTIMIZATION_TARGET_MODEL_VALIDATION, model_file_path);
    task_environment_.RunUntilIdle();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<ModelValidatorModelObserverTracker> model_observer_tracker_;
};

TEST_F(ModelValidatorExecutorTest, ValidModel) {
  base::FilePath model_file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path);
  model_file_path = model_file_path.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("optimization_guide")
                        .AppendASCII("simple_test.tflite");
  ValidateModel(model_file_path);

  // |ModelValidatorExecutor::Preprocess| returns an unimplemented error,
  // resulting in an unknown error in the final execution step.
  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus." +
          GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      ExecutionStatus::kErrorUnknown, 1);

  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
          GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      true, 1);

  histogram_tester().ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ModelLoadingDuration2." +
          GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      1);
}

// TODO(crbug.com/40194301): Enable this invalid model handling test once tflite
// error reporter msan failure is fixed.
TEST_F(ModelValidatorExecutorTest, DISABLED_InvalidModel) {
  base::ScopedTempDir model_dir;
  EXPECT_TRUE(model_dir.CreateUniqueTempDir());
  base::FilePath invalid_model_file_path =
      model_dir.GetPath().AppendASCII("invalid_model.tflite");
  base::WriteFile(invalid_model_file_path, "INVALID MODEL DATA");
  ValidateModel(invalid_model_file_path);

  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus." +
          GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      ExecutionStatus::kErrorModelFileNotValid, 1);

  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
          GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      false, 1);

  histogram_tester().ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ModelLoadingDuration2." +
          GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION),
      1);
}

}  // namespace optimization_guide
