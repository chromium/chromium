// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_executor.h"

#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/base_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace optimization_guide {

class TestModelExecutor
    : public BaseModelExecutor<std::vector<float>, const std::vector<float>&> {
 public:
  TestModelExecutor() = default;
  ~TestModelExecutor() override = default;

 protected:
  absl::Status Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                          const std::vector<float>& input) override {
    tflite::task::core::PopulateTensor<float>(input, input_tensors[0]);
    return absl::OkStatus();
  }

  std::vector<float> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override {
    std::vector<float> data;
    tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
    return data;
  }
};

class TestModelExecutorHandle
    : public ModelHandler<std::vector<float>, const std::vector<float>&> {
 public:
  explicit TestModelExecutorHandle(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : ModelHandler<std::vector<float>, const std::vector<float>&>(
            model_provider,
            background_task_runner,
            std::make_unique<TestModelExecutor>(),
            proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/absl::nullopt) {}
  ~TestModelExecutorHandle() override = default;
  TestModelExecutorHandle(const TestModelExecutorHandle&) = delete;
  TestModelExecutorHandle& operator=(const TestModelExecutorHandle&) = delete;

  // There is a method on the base class that exposes the returned supported
  // features, if provided by the loaded model received from the server.
  // absl::optional<proto::Any> supported_features_for_loaded_model();
};

class ModelObserverTracker : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    // Make sure we send what is expected based on
    // TestModelExecutorHandle ctor.
    if (target !=
        proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) {
      return;
    }
    if (model_metadata != absl::nullopt)
      return;

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

class BaseModelExecutorTest : public testing::Test {
 public:
  BaseModelExecutorTest() = default;
  ~BaseModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("optimization_guide")
                           .AppendASCII("simple_test.tflite");

    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
  }

  void TearDown() override { ResetModelExecutor(); }

  void CreateModelExecutor() {
    if (model_executor_handle_)
      model_executor_handle_.reset();

    model_executor_handle_ = std::make_unique<TestModelExecutorHandle>(
        model_observer_tracker_.get(),
        task_environment_.GetMainThreadTaskRunner());
  }

  void ResetModelExecutor() {
    model_executor_handle_.reset();
    // Allow for the background class to be destroyed.
    RunUntilIdle();
  }

  void PushModelFileToModelExecutor(
      proto::OptimizationTarget optimization_target,
      const absl::optional<proto::Any>& model_metadata) {
    DCHECK(model_executor_handle_);
    model_executor_handle_->OnModelFileUpdated(
        optimization_target, model_metadata, model_file_path_);
    RunUntilIdle();
  }

  TestModelExecutorHandle* model_executor_handle() {
    return model_executor_handle_.get();
  }

  ModelObserverTracker* model_observer_tracker() {
    return model_observer_tracker_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  base::FilePath model_file_path_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;

  std::unique_ptr<TestModelExecutorHandle> model_executor_handle_;
};

class ModelExecutorTest : public BaseModelExecutorTest {
 public:
  ModelExecutorTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kLoadModelFileForEachExecution);
  }
  ~ModelExecutorTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ModelExecutorTest, ObserverIsAttachedCorrectly) {
  CreateModelExecutor();
  EXPECT_TRUE(model_observer_tracker()->add_observer_called());

  ResetModelExecutor();
  EXPECT_TRUE(model_observer_tracker()->remove_observer_called());
}

TEST_F(ModelExecutorTest, ModelFileUpdatedWrongTarget) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
      /*model_metadata=*/absl::nullopt);

  EXPECT_FALSE(model_executor_handle()->ModelAvailable());
}

TEST_F(ModelExecutorTest, ModelFileUpdatedCorrectTarget) {
  base::HistogramTester histogram_tester;
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);

  EXPECT_TRUE(model_executor_handle()->ModelAvailable());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ModelExecutor.ModelLoadingResult." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      optimization_guide::ModelExecutorLoadingState::
          kModelFileValidAndMemoryMapped,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ModelLoadingDuration." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
}

TEST_F(ModelExecutorTest, ExecuteReturnsImmediatelyIfNoModelLoaded) {
  base::HistogramTester histogram_tester;
  CreateModelExecutor();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      std::vector<float>{1, 1, 1});
  run_loop->Run();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      false, 1);
}

TEST_F(ModelExecutorTest, ExecuteWithLoadedModel) {
  base::HistogramTester histogram_tester;
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  std::vector<float> input;
  int expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (int i = 0; i < expected_dims; i++)
    input.emplace_back(1);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());

            std::vector<float> expected_output = {
                -0.4936581, -0.32497078, -0.1705023, -0.38193324, 0.36136785,
                0.2177353,  0.32200375,  0.28686714, -0.21846706, -0.4200018};
            for (size_t i = 0; i < expected_output.size(); i++)
              EXPECT_NEAR(expected_output[i], output.value()[i], 1e-5);

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
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);
}

TEST_F(ModelExecutorTest, ExecuteTwiceWithLoadedModel) {
  base::HistogramTester histogram_tester;
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  std::vector<float> input;
  int expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (int i = 0; i < expected_dims; i++)
    input.emplace_back(1);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  // First run.
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TimeSincePreviousRun." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);

  // Second run.
  run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TimeSincePreviousRun." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
}

TEST_F(ModelExecutorTest, ParsedSupportedFeaturesForLoadedModelNoMetadata) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  EXPECT_FALSE(model_executor_handle()
                   ->ParsedSupportedFeaturesForLoadedModel<proto::Duration>()
                   .has_value());
}

TEST_F(ModelExecutorTest, ParsedSupportedFeaturesForLoadedModelWithMetadata) {
  CreateModelExecutor();

  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Duration");
  proto::Duration model_metadata;
  model_metadata.set_seconds(123);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      any_metadata);
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  absl::optional<proto::Duration> supported_features_for_loaded_model =
      model_executor_handle()
          ->ParsedSupportedFeaturesForLoadedModel<proto::Duration>();
  ASSERT_TRUE(supported_features_for_loaded_model.has_value());
  EXPECT_EQ(123, supported_features_for_loaded_model->seconds());
}

class ModelExecutorWithModelLoadingTest : public BaseModelExecutorTest {
 public:
  ModelExecutorWithModelLoadingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kLoadModelFileForEachExecution);
  }
  ~ModelExecutorWithModelLoadingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ModelExecutorWithModelLoadingTest, LoadModelFileForEachExecution) {
  base::HistogramTester histogram_tester;
  CreateModelExecutor();

  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Duration");
  proto::Duration model_metadata;
  model_metadata.set_seconds(123);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      any_metadata);

  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  // While the model isn't actually loaded yet, the supported features are
  // already known and do not change when the model is loaded or unloaded.
  EXPECT_TRUE(model_executor_handle()->supported_features_for_loaded_model());

  std::vector<float> input;
  size_t expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (size_t i = 0; i < expected_dims; i++) {
    input.emplace_back(1);
  }
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  RunUntilIdle();
  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  // After execution, the model should be unloaded in a PostTask, but the
  // metadata should still be available.

  EXPECT_TRUE(model_executor_handle()->supported_features_for_loaded_model());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);

  // After execution, the model should be unloaded in a PostTask, so give it a
  // change to do so.
  RunUntilIdle();

  // Run again and expect a second model load histogram count.
  run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 2);
}

}  // namespace optimization_guide
