// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace segmentation_platform {

namespace {

class ModelObserverTracker
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    registered_model_observers_.insert_or_assign(
        target, std::make_pair(model_metadata, observer));
  }

  bool DidRegisterForTarget(proto::SegmentId target) const {
    auto it = registered_model_observers_.find(target);
    if (it == registered_model_observers_.end()) {
      return false;
    }
    const auto& model_metadata = registered_model_observers_.at(target).first;

    EXPECT_TRUE(model_metadata);
    std::optional<proto::SegmentationModelMetadata> metadata =
        optimization_guide::ParsedAnyMetadata<proto::SegmentationModelMetadata>(
            model_metadata.value());
    EXPECT_TRUE(metadata);
    EXPECT_EQ(metadata->version_info().metadata_cur_version(),
              proto::CurrentVersion::METADATA_VERSION);

    return true;
  }

  optimization_guide::OptimizationTargetModelObserver* GetObserverForTarget(
      proto::SegmentId target) const {
    auto it = registered_model_observers_.find(target);
    if (it == registered_model_observers_.end()) {
      return nullptr;
    }
    return registered_model_observers_.at(target).second;
  }

 private:
  base::flat_map<
      optimization_guide::proto::OptimizationTarget,
      std::pair<std::optional<optimization_guide::proto::Any>,
                optimization_guide::OptimizationTargetModelObserver*>>
      registered_model_observers_;
};

}  // namespace

class OptimizationGuideSegmentationModelProviderTest : public testing::Test {
 public:
  OptimizationGuideSegmentationModelProviderTest() = default;
  ~OptimizationGuideSegmentationModelProviderTest() override = default;

  void SetUp() override {
    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
  }

  void TearDown() override {
    model_observer_tracker_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  std::unique_ptr<OptimizationGuideSegmentationModelProvider>
  CreateModelProvider(proto::SegmentId target) {
    return std::make_unique<OptimizationGuideSegmentationModelProvider>(
        model_observer_tracker_.get(),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
        target);
  }

  std::unique_ptr<optimization_guide::ModelInfo>
  CreateOptGuideModelInfoWithSegmentationMetadata() {
    proto::SegmentationModelMetadata metadata;
    std::string serialized_metadata;
    metadata.SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any_proto;
    auto any = std::make_optional(any_proto);
    any->set_value(serialized_metadata);
    any->set_type_url(
        "type.googleapis.com/"
        "segmentation_platform.proto.SegmentationModelMetadata");
    return optimization_guide::TestModelInfoBuilder()
        .SetModelMetadata(any)
        .Build();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
};

TEST_F(OptimizationGuideSegmentationModelProviderTest, InitAndFetchModel) {
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider =
      CreateModelProvider(
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  // Not initialized yet.
  EXPECT_FALSE(model_observer_tracker_->DidRegisterForTarget(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  // Init should register observer.
  provider->InitAndFetchModel(base::DoNothing());
  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  // Different target does not register yet.
  EXPECT_FALSE(model_observer_tracker_->DidRegisterForTarget(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE));

  // Initialize voice provider.
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider2 =
      CreateModelProvider(
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
  provider2->InitAndFetchModel(base::DoNothing());

  // 2 observers should be available:
  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
}

TEST_F(OptimizationGuideSegmentationModelProviderTest,
       ExecuteModelWithoutFetch) {
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider =
      CreateModelProvider(
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  base::RunLoop run_loop;
  ModelProvider::Request input = {4, 5};
  provider->ExecuteModelWithInput(
      input, base::BindOnce(
                 [](base::RunLoop* run_loop,
                    const std::optional<ModelProvider::Response>& output) {
                   EXPECT_FALSE(output.has_value());
                   run_loop->Quit();
                 },
                 &run_loop));
  run_loop.Run();
  RunUntilIdle();
}

TEST_F(OptimizationGuideSegmentationModelProviderTest, ExecuteModelWithFetch) {
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider =
      CreateModelProvider(
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  provider->InitAndFetchModel(base::DoNothing());
  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  base::RunLoop run_loop;
  ModelProvider::Request input = {4, 5};
  provider->ExecuteModelWithInput(
      input, base::BindOnce(
                 [](base::RunLoop* run_loop,
                    const std::optional<ModelProvider::Response>& output) {
                   // TODO(ssid): Consider using a mock executor to return
                   // results. This failure is caused by not no TFLite model
                   // being loaded in the opt-guide executor.
                   EXPECT_FALSE(output.has_value());
                   run_loop->Quit();
                 },
                 &run_loop));
  run_loop.Run();
  RunUntilIdle();
}

TEST_F(OptimizationGuideSegmentationModelProviderTest, NotifyOnDeletedModel) {
  base::MockCallback<ModelProvider::ModelUpdatedCallback>
      model_updated_callback;
  std::optional<proto::SegmentationModelMetadata> updated_model_metadata;
  EXPECT_CALL(model_updated_callback, Run(_, _, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&updated_model_metadata));

  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider =
      CreateModelProvider(
          proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  provider->InitAndFetchModel(model_updated_callback.Get());

  std::unique_ptr<optimization_guide::ModelInfo> model_info =
      CreateOptGuideModelInfoWithSegmentationMetadata();

  auto* model_observer = model_observer_tracker_->GetObserverForTarget(
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  // Invoke the model updated observer, model should now be available and the
  // event should be propagated to the rest of Segmentation Platform.
  model_observer->OnModelUpdated(
      optimization_guide::proto::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      *model_info);

  EXPECT_TRUE(provider->ModelAvailable());
  EXPECT_TRUE(updated_model_metadata.has_value());

  // Invoke the model updated observer again with no model, this happens when
  // the server is no longer serving a model for this optimization target. Model
  // availability should be reset and segmentation platform should be informed.
  model_observer->OnModelUpdated(
      optimization_guide::proto::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      std::nullopt);

  EXPECT_FALSE(provider->ModelAvailable());
  EXPECT_FALSE(updated_model_metadata.has_value());
}

}  // namespace segmentation_platform
