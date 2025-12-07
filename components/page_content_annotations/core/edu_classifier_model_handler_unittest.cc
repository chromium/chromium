// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/edu_classifier_model_handler.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {

class EduClassifierModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& any,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER) {
      base::FilePath test_data_dir;
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
      test_data_dir = test_data_dir.AppendASCII(
          "components/test/data/page_content_annotations");
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(test_data_dir.AppendASCII(
                                    "edu_classifier.tflite"))
                                .Build();
      observer->OnModelUpdated(optimization_target, *model_metadata);
    }
  }
};

class EduClassifierModelHandlerTest : public testing::Test {
 public:
  EduClassifierModelHandlerTest() = default;
  ~EduClassifierModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<EduClassifierModelProvider>();
    model_handler_ = std::make_unique<EduClassifierModelHandler>(
        model_provider_.get(), task_environment_.GetMainThreadTaskRunner());
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();
    // Wait for any DeleteSoon tasks to run.
    task_environment_.RunUntilIdle();
  }

  EduClassifierModelHandler* model_handler() const {
    return model_handler_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<EduClassifierModelHandler> model_handler_;
};

TEST_F(EduClassifierModelHandlerTest, ModelExecutes) {
  EduClassifierModelHandler* handler = model_handler();

  std::vector<float> input(768, 0.0f);

  base::test::TestFuture<const std::optional<float>&> test_future;
  handler->ExecuteModelWithInput(test_future.GetCallback(), input);

  ASSERT_TRUE(test_future.Wait());
  ASSERT_TRUE(test_future.Get().has_value());
}

}  // namespace

}  // namespace page_content_annotations
