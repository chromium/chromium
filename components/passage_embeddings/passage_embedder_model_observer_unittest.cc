// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embedder_model_observer.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/passage_embeddings/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace passage_embeddings {

class FakePassageEmbeddingsServiceController
    : public passage_embeddings::PassageEmbeddingsServiceController {
 public:
  explicit FakePassageEmbeddingsServiceController(
      base::test::TestFuture<bool>* model_info_future)
      : model_info_received_future_(model_info_future) {}
  ~FakePassageEmbeddingsServiceController() override = default;

  // passage_embeddings::PassageEmbeddingsServiceController:
  bool MaybeUpdateModelInfo(
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override {
    const bool received_model_info = model_info.has_value();
    model_info_received_future_->SetValue(received_model_info);
    return received_model_info;
  }
  void MaybeLaunchService() override {}
  void ResetServiceRemote() override {}

 protected:
  raw_ptr<base::test::TestFuture<bool>> model_info_received_future_;
};

class TestOptimizationGuideModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  explicit TestOptimizationGuideModelProvider(
      base::test::TestFuture<bool>* target_observed_future)
      : target_observed_future_(target_observed_future) {}

  // optimization_guide::OptimizationGuideModelProvider:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    target_observed_future_->SetValue(
        optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER);
    observer_list_.AddObserver(observer);
    NotifyObservers();
  }
  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  // Set the model info to be sent to the observer.
  void SetModelInfo(std::unique_ptr<optimization_guide::ModelInfo> model_info) {
    model_info_ = std::move(model_info);
    NotifyObservers();
  }

 private:
  void NotifyObservers() {
    if (model_info_) {
      observer_list_.Notify(
          &optimization_guide::OptimizationTargetModelObserver::OnModelUpdated,
          optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
          *model_info_);
    } else {
      observer_list_.Notify(
          &optimization_guide::OptimizationTargetModelObserver::OnModelUpdated,
          optimization_guide::proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER,
          std::nullopt);
    }
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<base::test::TestFuture<bool>> target_observed_future_;
  base::ObserverList<optimization_guide::OptimizationTargetModelObserver>
      observer_list_;
  std::unique_ptr<optimization_guide::ModelInfo> model_info_;
};

class PassageEmbedderModelObserverTest : public testing::Test {
 protected:
  base::test::TestFuture<bool> target_observed_future_;
  base::test::TestFuture<bool> model_info_received_future_;
};

TEST_F(PassageEmbedderModelObserverTest, ObservesTargetAndNotifiesObserver) {
  auto model_provider = std::make_unique<TestOptimizationGuideModelProvider>(
      &target_observed_future_);

  EXPECT_FALSE(target_observed_future_.IsReady());

  auto service_controller =
      std::make_unique<FakePassageEmbeddingsServiceController>(
          &model_info_received_future_);

  EXPECT_FALSE(model_info_received_future_.IsReady());

  auto passage_embedder_model_observer =
      std::make_unique<PassageEmbedderModelObserver>(model_provider.get(),
                                                     service_controller.get());

  EXPECT_TRUE(target_observed_future_.IsReady());
  EXPECT_TRUE(target_observed_future_.Take());

  EXPECT_TRUE(model_info_received_future_.IsReady());
  EXPECT_FALSE(model_info_received_future_.Take());

  model_provider->SetModelInfo(GetBuilderWithValidModelInfo().Build());
  EXPECT_TRUE(model_info_received_future_.IsReady());
  EXPECT_TRUE(model_info_received_future_.Take());
}

}  // namespace passage_embeddings
