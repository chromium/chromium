// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_model_service.h"

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAreArray;

namespace {

static const char kTailModelFilename[] = "test_tail_model.tflite";
static const char kVocabFilename[] = "vocab_test.txt";

constexpr int kNumLayer = 1;
constexpr int kStateSize = 512;
constexpr int kEmbeddingDim = 64;
constexpr int kMaxNumSteps = 20;
constexpr float kProbabilityThreshold = 0.01;

}  // namespace

class OnDeviceTailModelServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    service_ =
        std::make_unique<OnDeviceTailModelService>(test_model_provider_.get());

    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.OnDeviceTailSuggestModelMetadata");

    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("components/test/data/omnibox");

    base::flat_set<base::FilePath> additional_files;
    additional_files.insert(test_data_dir.AppendASCII(kVocabFilename));

    optimization_guide::proto::OnDeviceTailSuggestModelMetadata metadata;
    metadata.mutable_lstm_model_params()->set_num_layer(kNumLayer);
    metadata.mutable_lstm_model_params()->set_state_size(kStateSize);
    metadata.mutable_lstm_model_params()->set_embedding_dimension(
        kEmbeddingDim);
    metadata.mutable_lstm_model_params()->set_max_num_steps(kMaxNumSteps);
    metadata.mutable_lstm_model_params()->set_probability_threshold(
        kProbabilityThreshold);
    metadata.SerializeToString(any_metadata.mutable_value());

    model_info_ =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(test_data_dir.AppendASCII(kTailModelFilename))
            .SetAdditionalFiles(additional_files)
            .SetVersion(123)
            .SetModelMetadata(any_metadata)
            .Build();

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    service_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  bool IsExecutorReady() const {
    return service_->tail_model_executor_->IsReady();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OnDeviceTailModelService> service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider_;
  std::unique_ptr<optimization_guide::ModelInfo> model_info_;
};

TEST_F(OnDeviceTailModelServiceTest, OnModelUpdated) {
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      *model_info_);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(IsExecutorReady());
}

TEST_F(OnDeviceTailModelServiceTest, GetPredictionsForInput) {
  std::vector<OnDeviceTailModelExecutor::Prediction> results;

  OnDeviceTailModelExecutor::ModelInput input("faceb", "", 5);
  OnDeviceTailModelService::ResultCallback callback = base::BindOnce(
      [](std::vector<OnDeviceTailModelExecutor::Prediction>* results,
         std::vector<OnDeviceTailModelExecutor::Prediction> predictions) {
        *results = std::move(predictions);
      },
      &results);

  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      *model_info_);
  service_->GetPredictionsForInput(input, std::move(callback));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(base::StartsWith(results[0].suggestion, "facebook",
                               base::CompareCase::SENSITIVE));
}

TEST_F(OnDeviceTailModelServiceTest, NullModelUpdate) {
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      *model_info_);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsExecutorReady());

  // Null model update should disable the executor.
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      std::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsExecutorReady());
}

TEST_F(OnDeviceTailModelServiceTest, MemoryPressureLevel) {
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      *model_info_);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsExecutorReady());

  OnDeviceTailModelExecutor::ModelInput input("faceb", "", 5);
  std::vector<OnDeviceTailModelExecutor::Prediction> results;
  memory_pressure::test::FakeMemoryPressureMonitor mem_pressure_monitor;

  // The executor should be unloaded from memory when memory pressure level is
  // critical.
  std::vector<OnDeviceTailModelExecutor::Prediction> results_1;
  OnDeviceTailModelService::ResultCallback callback_1 = base::BindOnce(
      [](std::vector<OnDeviceTailModelExecutor::Prediction>* results,
         std::vector<OnDeviceTailModelExecutor::Prediction> predictions) {
        *results = std::move(predictions);
      },
      &results_1);
  mem_pressure_monitor.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  service_->GetPredictionsForInput(input, std::move(callback_1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsExecutorReady());
  EXPECT_TRUE(results_1.empty());

  // The executor should then be re-initialized once pressure level drops.
  std::vector<OnDeviceTailModelExecutor::Prediction> results_2;
  OnDeviceTailModelService::ResultCallback callback_2 = base::BindOnce(
      [](std::vector<OnDeviceTailModelExecutor::Prediction>* results,
         std::vector<OnDeviceTailModelExecutor::Prediction> predictions) {
        *results = std::move(predictions);
      },
      &results_2);
  mem_pressure_monitor.SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);
  service_->GetPredictionsForInput(input, std::move(callback_2));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsExecutorReady());
  EXPECT_FALSE(results_2.empty());
}
