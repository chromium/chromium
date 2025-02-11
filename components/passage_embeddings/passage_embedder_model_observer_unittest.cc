// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embedder_model_observer.h"

#include <memory>

#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/passage_embeddings/embedder.h"
#include "components/passage_embeddings/mock_embedder.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

class FakePassageEmbeddingsServiceController
    : public passage_embeddings::PassageEmbeddingsServiceController {
 public:
  FakePassageEmbeddingsServiceController() = default;
  ~FakePassageEmbeddingsServiceController() override = default;
  void MaybeLaunchService() override {}
  void ResetServiceRemote() override {}
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

    if (!model_info_) {
      observer->OnModelUpdated(
          optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
          std::nullopt);
    } else {
      observer->OnModelUpdated(
          optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
          *model_info_);
    }
  }

  bool passage_embedder_target_registered() const {
    return passage_embedder_target_registered_;
  }

  // Set the model info to be sent to the observer.
  void SetModelInfo(std::unique_ptr<optimization_guide::ModelInfo> model_info) {
    model_info_ = std::move(model_info);
  }

 private:
  bool passage_embedder_target_registered_ = false;
  std::unique_ptr<optimization_guide::ModelInfo> model_info_;
};

class PassageEmbedderModelObserverTest : public testing::Test {};

TEST_F(PassageEmbedderModelObserverTest, ObservesTarget) {
  auto model_provider = std::make_unique<TestOptimizationGuideModelProvider>();
  auto service_controller =
      std::make_unique<FakePassageEmbeddingsServiceController>();

  EXPECT_FALSE(model_provider->passage_embedder_target_registered());
  auto passage_embedder_model_observer =
      std::make_unique<PassageEmbedderModelObserver>(
          model_provider.get(), service_controller.get(), false);
  EXPECT_TRUE(model_provider->passage_embedder_target_registered());
}

}  // namespace passage_embeddings
