// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_embedder.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/history_embeddings/passage_embeddings_service_controller.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "services/passage_embeddings/passage_embeddings_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

class FakePassageEmbeddingsServiceController
    : public PassageEmbeddingsServiceController {
 public:
  FakePassageEmbeddingsServiceController() = default;

  void LaunchService() override {
    did_launch_service_ = true;
    service_remote_.reset();
    service_ = std::make_unique<passage_embeddings::PassageEmbeddingsService>(
        service_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  ~FakePassageEmbeddingsServiceController() override = default;

  std::unique_ptr<passage_embeddings::PassageEmbeddingsService> service_;
  bool did_launch_service_ = false;
};

class TestOptimizationGuideModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER) {
      passage_embedder_target_registered_ = true;
    }
  }

  bool passage_embedder_target_registered() const {
    return passage_embedder_target_registered_;
  }

 private:
  bool passage_embedder_target_registered_ = false;
};

}  // namespace

class MlEmbedderTest : public testing::Test {
 public:
  void SetUp() override {
    model_provider_ = std::make_unique<TestOptimizationGuideModelProvider>();
  }

  void TearDown() override {}

 protected:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestOptimizationGuideModelProvider> model_provider_;
};

TEST_F(MlEmbedderTest, RegistersForTarget) {
  auto ml_embedder = std::make_unique<MlEmbedder>(
      model_provider_.get(), /*service_controller=*/nullptr);

  EXPECT_TRUE(model_provider_->passage_embedder_target_registered());
}

}  // namespace history_embeddings
