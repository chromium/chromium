// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"

#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

class ModelObserverTracker
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget target,
      const absl::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    registered_model_metadata_.insert_or_assign(target, model_metadata);
  }

  bool DidRegisterForTarget(proto::SegmentId target) const {
    auto it = registered_model_metadata_.find(target);
    if (it == registered_model_metadata_.end())
      return false;
    const auto& model_metadata = registered_model_metadata_.at(target);

    EXPECT_TRUE(model_metadata);
    absl::optional<proto::SegmentationModelMetadata> metadata =
        optimization_guide::ParsedAnyMetadata<proto::SegmentationModelMetadata>(
            model_metadata.value());
    EXPECT_TRUE(metadata);
    EXPECT_EQ(metadata->version_info().metadata_cur_version(),
              proto::CurrentVersion::METADATA_VERSION);

    return true;
  }

 private:
  base::flat_map<optimization_guide::proto::OptimizationTarget,
                 absl::optional<optimization_guide::proto::Any>>
      registered_model_metadata_;
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
                    const absl::optional<ModelProvider::Response>& output) {
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
                    const absl::optional<ModelProvider::Response>& output) {
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

}  // namespace segmentation_platform
