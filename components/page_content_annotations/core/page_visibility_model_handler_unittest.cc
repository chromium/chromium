// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_visibility_model_handler.h"

#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

class PageVisibilityModelHandlerTest : public testing::Test {
 public:
  PageVisibilityModelHandlerTest() = default;
  ~PageVisibilityModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    model_handler_ = std::make_unique<PageVisibilityModelHandler>(
        model_provider_.get(), task_environment_.GetMainThreadTaskRunner(),
        /*model_metadata=*/std::nullopt);
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();
    RunUntilIdle();
  }

  PageVisibilityModelHandler* model_handler() const {
    return model_handler_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<PageVisibilityModelHandler> model_handler_;
};

TEST_F(PageVisibilityModelHandlerTest, NotSensitiveNotFound) {
  PageVisibilityModelHandler* executor = model_handler();

  std::optional<double> visibility_score =
      executor->ExtractContentVisibilityFromModelOutput(
          std::vector<tflite::task::core::Category>{
              {"BLAH-BLAH-BLAH", 0.3},
              {"SENSITIVE", 0.7},
          });

  EXPECT_FALSE(visibility_score);
}

TEST_F(PageVisibilityModelHandlerTest, HasScore) {
  PageVisibilityModelHandler* executor = model_handler();

  std::optional<double> visibility_score =
      executor->ExtractContentVisibilityFromModelOutput(
          std::vector<tflite::task::core::Category>{
              {"NOT-SENSITIVE", 0.9},
              {"SENSITIVE", 0.1},
          });

  ASSERT_TRUE(visibility_score);
  EXPECT_EQ(*visibility_score, 0.9);
}

}  // namespace page_content_annotations
