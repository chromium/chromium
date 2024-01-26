// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/test_model_executor.h"
#include "components/optimization_guide/core/test_model_handler.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

class ModelObserverTracker : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const std::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    // Make sure we send what is expected based on
    // TestModelHandler ctor.
    if (target !=
        proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) {
      return;
    }
    if (model_metadata != std::nullopt) {
      return;
    }

    add_observer_called_ = true;
  }

  bool add_observer_called() const { return add_observer_called_; }

  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      OptimizationTargetModelObserver* observer) override {
    if (target !=
        proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) {
      return;
    }
    remove_observer_called_ = true;
  }

  bool remove_observer_called() const { return remove_observer_called_; }

 private:
  bool add_observer_called_ = false;
  bool remove_observer_called_ = false;
};

class ModelHandlerTest : public testing::Test {
 public:
  ModelHandlerTest() = default;
  ~ModelHandlerTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("optimization_guide")
                           .AppendASCII("simple_test.tflite");

    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
  }

  void TearDown() override { ResetModelHandler(); }

  void CreateModelHandler() {
    if (model_handler_)
      model_handler_.reset();

    model_handler_ = std::make_unique<TestModelHandler>(
        model_observer_tracker(), task_environment_.GetMainThreadTaskRunner());
  }

  void ResetModelHandler(std::unique_ptr<TestModelHandler> handle = nullptr) {
    model_handler_ = std::move(handle);
    // Allow for the background class to be destroyed.
    RunUntilIdle();
  }

  void PushModelFileToModelExecutor(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata) {
    DCHECK(model_handler());
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(model_file_path_)
            .SetModelMetadata(model_metadata)
            .Build();
    model_handler()->OnModelUpdated(optimization_target, *model_info);
    RunUntilIdle();
  }

  TestModelHandler* model_handler() { return model_handler_.get(); }

  ModelObserverTracker* model_observer_tracker() {
    return model_observer_tracker_.get();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  base::FilePath model_file_path_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;

  std::unique_ptr<TestModelHandler> model_handler_;
};

TEST_F(ModelHandlerTest, ObserverIsAttachedCorrectly) {
  base::HistogramTester histogram_tester;

  CreateModelHandler();
  EXPECT_TRUE(model_observer_tracker()->add_observer_called());

  ResetModelHandler();
  EXPECT_TRUE(model_observer_tracker()->remove_observer_called());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelHandler.HandlerCreated.PainfulPageLoad", true, 1);
}

TEST_F(ModelHandlerTest, ModelFileUpdatedWrongTarget) {
  base::HistogramTester histogram_tester;

  CreateModelHandler();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
      /*model_metadata=*/std::nullopt);

  EXPECT_FALSE(model_handler()->ModelAvailable());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelHandler.HandlerCreated.PainfulPageLoad", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelHandler.HandlerCreatedToModelAvailable."
      "PainfulPageLoad",
      0);
}

TEST_F(ModelHandlerTest, ParsedSupportedFeaturesForLoadedModelNoMetadata) {
  base::HistogramTester histogram_tester;

  CreateModelHandler();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/std::nullopt);
  EXPECT_TRUE(model_handler()->ModelAvailable());
  EXPECT_TRUE(model_handler()->GetModelInfo());

  EXPECT_FALSE(model_handler()
                   ->ParsedSupportedFeaturesForLoadedModel<proto::Duration>()
                   .has_value());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelHandler.HandlerCreated.PainfulPageLoad", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelHandler.HandlerCreatedToModelAvailable."
      "PainfulPageLoad",
      1);
}

TEST_F(ModelHandlerTest, MultipleModelUpdatesOnlyRecordsMetricOnce) {
  base::HistogramTester histogram_tester;

  CreateModelHandler();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/std::nullopt);
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/std::nullopt);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelHandler.HandlerCreated.PainfulPageLoad", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelHandler.HandlerCreatedToModelAvailable."
      "PainfulPageLoad",
      1);
}

TEST_F(ModelHandlerTest, ParsedSupportedFeaturesForLoadedModelWithMetadata) {
  CreateModelHandler();

  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Duration");
  proto::Duration model_metadata;
  model_metadata.set_seconds(123);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      any_metadata);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  std::optional<proto::Duration> supported_features_for_loaded_model =
      model_handler()->ParsedSupportedFeaturesForLoadedModel<proto::Duration>();
  ASSERT_TRUE(supported_features_for_loaded_model.has_value());
  EXPECT_EQ(123, supported_features_for_loaded_model->seconds());
  EXPECT_TRUE(model_handler()->GetModelInfo());
}

TEST_F(ModelHandlerTest, Execute) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  std::vector<float> input;
  input.push_back(1.0f);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            EXPECT_EQ((size_t)1, output.value().size());
            EXPECT_EQ(1.0f, output.value().at(0));

            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
}

TEST_F(ModelHandlerTest, ExecuteWithCancelableTaskTracker) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  base::CancelableTaskTracker task_tracker;

  std::vector<float> input;
  input.push_back(1.0f);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
      &task_tracker,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            EXPECT_EQ((size_t)1, output.value().size());
            EXPECT_EQ(1.0f, output.value().at(0));

            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
}

TEST_F(ModelHandlerTest, ExecuteWithCancelableTaskTrackerCanceled) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  base::CancelableTaskTracker task_tracker;

  std::vector<float> input;
  input.push_back(1.0f);

  bool task_completed = false;
  model_handler()->ExecuteModelWithInput(
      &task_tracker,
      base::BindOnce(
          [](bool* task_completed,
             const std::optional<std::vector<float>>& output) {
            *task_completed = true;
          },
          &task_completed),
      input);
  task_tracker.TryCancelAll();

  ASSERT_FALSE(task_completed);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
}

TEST_F(ModelHandlerTest, BatchExecuteModelWithInputSync) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  std::vector<float> input;
  input.push_back(1.0f);

  auto batch_outputs =
      model_handler()->BatchExecuteModelWithInputSync({input, input});
  EXPECT_EQ(2u, batch_outputs.size());
  for (const auto& output : batch_outputs) {
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(1u, output.value().size());
    EXPECT_EQ(1.0f, output.value().at(0));
  }

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
}

TEST_F(ModelHandlerTest, AddOnModelUpdatedCallback_RunsImmediately) {
  CreateModelHandler();

  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Duration");
  proto::Duration model_metadata;
  model_metadata.set_seconds(123);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      any_metadata);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  bool callback_run = false;
  model_handler()->AddOnModelUpdatedCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_run));

  EXPECT_TRUE(callback_run);
}

TEST_F(ModelHandlerTest, AddOnModelUpdatedCallback_RunsOnUpdate) {
  CreateModelHandler();

  bool callback_run = false;
  model_handler()->AddOnModelUpdatedCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_run));
  EXPECT_FALSE(callback_run);

  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Duration");
  proto::Duration model_metadata;
  model_metadata.set_seconds(123);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      any_metadata);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  EXPECT_TRUE(callback_run);
}

TEST_F(ModelHandlerTest, AddOnModelUpdatedCallback_NeverRun) {
  CreateModelHandler();

  bool callback_run = false;
  model_handler()->AddOnModelUpdatedCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_run));
  EXPECT_FALSE(callback_run);

  // Resets model_handler
  CreateModelHandler();

  EXPECT_FALSE(callback_run);
}

}  // namespace
}  // namespace optimization_guide
