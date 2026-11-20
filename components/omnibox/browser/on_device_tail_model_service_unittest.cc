// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_model_service.h"

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/memory_coordinator/utils.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#include "components/omnibox/browser/on_device_tail_model_executor_constants.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAreArray;

namespace {

static const char kTailModelFilename[] = "test_tail_model.tflite";
static const char kVocabFilename[] = "vocab_test.txt";

constexpr int kNumLayer = 1;
constexpr int kStateSize = 512;
constexpr int kEmbeddingDim = 64;

using Predictions = std::vector<OnDeviceTailModelExecutor::Prediction>;

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

    std::vector<base::FilePath> additional_files = {
        test_data_dir.AppendASCII(kVocabFilename)};

    optimization_guide::proto::OnDeviceTailSuggestModelMetadata metadata;
    metadata.mutable_lstm_model_params()->set_num_layer(kNumLayer);
    metadata.mutable_lstm_model_params()->set_state_size(kStateSize);
    metadata.mutable_lstm_model_params()->set_embedding_dimension(
        kEmbeddingDim);
    metadata.mutable_lstm_model_params()->set_max_num_steps(
        kDefaultMaxNumSteps);
    metadata.mutable_lstm_model_params()->set_probability_threshold(
        kDefaultProbabilityThreshold);
    metadata.SerializeToString(any_metadata.mutable_value());

    model_info_ = optimization_guide::ModelInfo{
        .model_file_path = test_data_dir.AppendASCII(kTailModelFilename),
        .additional_files = additional_files,
        .version = 123,
        .model_metadata = any_metadata,
    };

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    service_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  bool IsExecutorReady() const {
    return service_->tail_model_executor_->IsReady();
  }

  // Delivers the model and runs it once, which loads it.
  void UpdateAndLoadModel() {
    service_->OnModelUpdated(
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
        model_info_);
    service_->GetPredictionsForInput(
        OnDeviceTailModelExecutor::ModelInput("faceb", "", 5),
        base::DoNothing());
    task_environment_.RunUntilIdle();
  }

  base::TestMemoryConsumerRegistry memory_consumer_registry_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OnDeviceTailModelService> service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider_;
  std::optional<optimization_guide::ModelInfo> model_info_;
};

TEST_F(OnDeviceTailModelServiceTest, ModelIsLoadedOnFirstUse) {
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      model_info_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsExecutorReady());

  service_->GetPredictionsForInput(
      OnDeviceTailModelExecutor::ModelInput("faceb", "", 5), base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsExecutorReady());
}

TEST_F(OnDeviceTailModelServiceTest, ValidationSwitchLoadsModelAtDelivery) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      "omnibox-on-device-tail-model-validation");
  base::HistogramTester histogram_tester;

  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      model_info_);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(IsExecutorReady());
  histogram_tester.ExpectUniqueSample("Omnibox.OnDeviceTailModel.InitExecutor",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.OnDeviceTailModel.HasResultForTestPrefix", true, 1);
}

TEST_F(OnDeviceTailModelServiceTest, ModelUpdateIsUsedByNextRequest) {
  OnDeviceTailModelExecutor::ModelInput input("logi", "", 5);
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      model_info_);
  base::test::TestFuture<Predictions> before;
  service_->GetPredictionsForInput(input, before.GetCallback());
  EXPECT_FALSE(before.Take().empty());
  EXPECT_TRUE(IsExecutorReady());

  // The denylist bans "ogi", so the updated files give no result for "logi".
  model_info_->additional_files.push_back(
      model_info_->model_file_path.DirName().AppendASCII("denylist_test.txt"));
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      model_info_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsExecutorReady());

  base::test::TestFuture<Predictions> after;
  service_->GetPredictionsForInput(input, after.GetCallback());
  EXPECT_TRUE(after.Take().empty());
  EXPECT_TRUE(IsExecutorReady());
}

TEST_F(OnDeviceTailModelServiceTest, LoadFailureOnFirstUse) {
  model_info_->model_file_path =
      model_info_->model_file_path.DirName().AppendASCII("no_such.tflite");
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      model_info_);

  OnDeviceTailModelExecutor::ModelInput input("faceb", "", 5);
  // The second request checks that requests after a failed load still reply.
  for (int i = 0; i < 2; ++i) {
    base::test::TestFuture<Predictions> future;
    service_->GetPredictionsForInput(input, future.GetCallback());
    EXPECT_TRUE(future.Take().empty());
    EXPECT_FALSE(IsExecutorReady());
  }
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
      model_info_);
  service_->GetPredictionsForInput(input, std::move(callback));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(base::StartsWith(results[0].suggestion, "facebook",
                               base::CompareCase::SENSITIVE));
}

TEST_F(OnDeviceTailModelServiceTest, NullModelUpdate) {
  UpdateAndLoadModel();
  EXPECT_TRUE(IsExecutorReady());

  // Null model update should disable the executor.
  service_->OnModelUpdated(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
      std::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsExecutorReady());

  // A later request must not load the removed model again.
  base::test::TestFuture<Predictions> future;
  service_->GetPredictionsForInput(
      OnDeviceTailModelExecutor::ModelInput("faceb", "", 5),
      future.GetCallback());
  EXPECT_TRUE(future.Take().empty());
  EXPECT_FALSE(IsExecutorReady());

  // A new model delivery makes the model usable again.
  UpdateAndLoadModel();
  EXPECT_TRUE(IsExecutorReady());
}

TEST_F(OnDeviceTailModelServiceTest, MemoryPressureLevel) {
  UpdateAndLoadModel();
  EXPECT_TRUE(IsExecutorReady());

  OnDeviceTailModelExecutor::ModelInput input("faceb", "", 5);

  // The executor should be unloaded from memory when memory pressure level is
  // critical.
  std::vector<OnDeviceTailModelExecutor::Prediction> results_1;
  OnDeviceTailModelService::ResultCallback callback_1 = base::BindOnce(
      [](std::vector<OnDeviceTailModelExecutor::Prediction>* results,
         std::vector<OnDeviceTailModelExecutor::Prediction> predictions) {
        *results = std::move(predictions);
      },
      &results_1);
  memory_consumer_registry_.NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::CriticalPressureThreshold());
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
  memory_consumer_registry_.NotifyUpdateMemoryLimitAndRelease(
      base::MemoryLimit::ModeratePressureThreshold());
  service_->GetPredictionsForInput(input, std::move(callback_2));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsExecutorReady());
  EXPECT_FALSE(results_2.empty());
}
