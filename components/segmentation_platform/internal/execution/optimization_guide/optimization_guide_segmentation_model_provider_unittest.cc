// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"

#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
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

  bool DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget target) const {
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
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
  }

  void TearDown() override {
    model_observer_tracker_.reset();
    task_runner_->RunPendingTasks();
  }

  std::unique_ptr<OptimizationGuideSegmentationModelProvider>
  CreateModelProvider(optimization_guide::proto::OptimizationTarget target) {
    return std::make_unique<OptimizationGuideSegmentationModelProvider>(
        model_observer_tracker_.get(), task_runner_, target);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
};

TEST_F(OptimizationGuideSegmentationModelProviderTest, InitAndFetchModel) {
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider =
      CreateModelProvider(optimization_guide::proto::OptimizationTarget::
                              OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  // Not initialized yet.
  EXPECT_FALSE(model_observer_tracker_->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  // Init should register observer.
  provider->InitAndFetchModel(base::DoNothing());
  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  // Different target does not register yet.
  EXPECT_FALSE(model_observer_tracker_->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_VOICE));

  // Initialize voice provider.
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider2 =
      CreateModelProvider(optimization_guide::proto::OptimizationTarget::
                              OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
  provider2->InitAndFetchModel(base::DoNothing());

  // 2 observers should be available:
  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
}

TEST_F(OptimizationGuideSegmentationModelProviderTest,
       ExecuteModelWithoutFetch) {
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider =
      CreateModelProvider(optimization_guide::proto::OptimizationTarget::
                              OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  base::RunLoop run_loop;
  std::vector<float> input = {4, 5};
  provider->ExecuteModelWithInput(
      input,
      base::BindOnce(
          [](base::RunLoop* run_loop, const absl::optional<float>& output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(OptimizationGuideSegmentationModelProviderTest, ExecuteModelWithFetch) {
  std::unique_ptr<OptimizationGuideSegmentationModelProvider> provider =
      CreateModelProvider(optimization_guide::proto::OptimizationTarget::
                              OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  provider->InitAndFetchModel(base::DoNothing());
  EXPECT_TRUE(model_observer_tracker_->DidRegisterForTarget(
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  base::RunLoop run_loop;
  std::vector<float> input = {4, 5};
  provider->ExecuteModelWithInput(
      input,
      base::BindOnce(
          [](base::RunLoop* run_loop, const absl::optional<float>& output) {
            // TODO(ssid): Consider using a mock executor to return results.
            // This failure is caused by not no TFLite model being loaded in the
            // opt-guide executor.
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          &run_loop));
  task_runner_->RunPendingTasks();
  run_loop.Run();
}

}  // namespace segmentation_platform
