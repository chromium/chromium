// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/optimization_target_model_executor.h"

#include "base/path_service.h"
#include "components/optimization_guide/content/browser/base_model_executor.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace optimization_guide {

class TestModelExecutor
    : public BaseModelExecutor<std::vector<float>, const std::vector<float>&> {
 public:
  // Feature team specifies their target and the features they support along
  // with a background task runner that has the appropriate properties for
  // executing models.
  TestModelExecutor(OptimizationGuideDecider* decider,
                    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : BaseModelExecutor<std::vector<float>, const std::vector<float>&>(
            decider,
            proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/base::nullopt,
            task_runner) {}
  ~TestModelExecutor() override = default;
  TestModelExecutor(const TestModelExecutor&) = delete;
  TestModelExecutor& operator=(const TestModelExecutor&) = delete;

  // There is a method on the base class that exposes the returned supported
  // features, if provided by the loaded model received from the server.
  // base::Optional<proto::Any> supported_features_for_loaded_model();

 protected:
  void Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const std::vector<float>& input) override {
    tflite::task::core::PopulateTensor<float>(input, input_tensors[0]);
  }

  std::vector<float> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override {
    std::vector<float> data;
    tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
    return data;
  }
};

class ModelObserverTracker : public TestOptimizationGuideDecider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const base::Optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    // Make sure we send what is expected based on
    // TestModelExecutor ctor.
    if (target !=
        proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) {
      return;
    }
    if (model_metadata != base::nullopt)
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

class OptimizationTargetModelExecutorTest : public testing::Test {
 public:
  OptimizationTargetModelExecutorTest() = default;
  ~OptimizationTargetModelExecutorTest() override = default;

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

  void TearDown() override { model_executor_.reset(); }

  void CreateModelExecutor() {
    if (model_executor_)
      model_executor_.reset();

    model_executor_ = std::make_unique<TestModelExecutor>(
        model_observer_tracker_.get(),
        task_environment_.GetMainThreadTaskRunner());
  }

  void ResetModelExecutor() { model_executor_.reset(); }

  void PushModelFileToModelExecutor(
      proto::OptimizationTarget optimization_target) {
    DCHECK(model_executor_);
    model_executor_->OnModelFileUpdated(optimization_target, base::nullopt,
                                        model_file_path_);
    RunUntilIdle();
  }

  TestModelExecutor* model_executor() { return model_executor_.get(); }

  ModelObserverTracker* model_observer_tracker() {
    return model_observer_tracker_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  base::FilePath model_file_path_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;

  std::unique_ptr<TestModelExecutor> model_executor_;
};

TEST_F(OptimizationTargetModelExecutorTest, ObserverIsAttachedCorrectly) {
  CreateModelExecutor();
  EXPECT_TRUE(model_observer_tracker()->add_observer_called());

  ResetModelExecutor();
  EXPECT_TRUE(model_observer_tracker()->remove_observer_called());
}

TEST_F(OptimizationTargetModelExecutorTest, ModelFileUpdatedWrongTarget) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION);

  EXPECT_FALSE(model_executor()->HasLoadedModel());
}

TEST_F(OptimizationTargetModelExecutorTest, ModelFileUpdatedCorrectTarget) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_TRUE(model_executor()->HasLoadedModel());
}

TEST_F(OptimizationTargetModelExecutorTest,
       ExecuteReturnsImmediatelyIfNoModelLoaded) {
  CreateModelExecutor();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const base::Optional<std::vector<float>>& output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      std::vector<float>{1, 1, 1});
  run_loop->Run();
}

TEST_F(OptimizationTargetModelExecutorTest, ExecuteWithLoadedModel) {
  CreateModelExecutor();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(model_executor()->HasLoadedModel());

  std::vector<float> input;
  int expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (int i = 0; i < expected_dims; i++)
    input.emplace_back(1);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const base::Optional<std::vector<float>>& output) {
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
}

}  // namespace optimization_guide
