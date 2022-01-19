// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/segmentation_model_executor.h"

#include <memory>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/segmentation_model_handler.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {
const auto kOptimizationTarget = optimization_guide::proto::OptimizationTarget::
    OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
}  // namespace

namespace segmentation_platform {
bool AreEqual(const proto::SegmentationModelMetadata& a,
              const proto::SegmentationModelMetadata& b) {
  // Serializing two protos and comparing them is unsafe, in particular if they
  // contain a map because the wire format of a proto is not guaranteed to be
  // constant. However, in practice this should work well for the simplistic
  // test case we are running here.
  std::string serialized_a = a.SerializeAsString();
  std::string serialized_b = b.SerializeAsString();
  return serialized_a == serialized_b;
}

class SegmentationModelExecutorTest : public testing::Test {
 public:
  SegmentationModelExecutorTest() = default;
  ~SegmentationModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("segmentation_platform")
                           .AppendASCII("adder.tflite");

    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
  }

  void TearDown() override { ResetModelExecutor(); }

  void CreateModelExecutor(
      SegmentationModelHandler::ModelUpdatedCallback callback) {
    if (model_executor_handle_)
      model_executor_handle_.reset();

    model_executor_handle_ = std::make_unique<SegmentationModelHandler>(
        optimization_guide_model_provider_.get(),
        task_environment_.GetMainThreadTaskRunner(), kOptimizationTarget,
        callback, absl::nullopt);
  }

  void ResetModelExecutor() {
    model_executor_handle_.reset();
    // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
    // to be destroyed.
    RunUntilIdle();
  }

  void PushModelFileToModelExecutor(
      absl::optional<proto::SegmentationModelMetadata> metadata) {
    absl::optional<optimization_guide::proto::Any> any;

    // Craft a correct Any proto in the case we passed in metadata.
    if (metadata.has_value()) {
      std::string serialized_metadata;
      (*metadata).SerializeToString(&serialized_metadata);
      optimization_guide::proto::Any any_proto;
      any = absl::make_optional(any_proto);
      any->set_value(serialized_metadata);
      // Need to set the type URL for ParsedSupportedFeaturesForLoadedModel() to
      // work correctly, since it's verifying the type name.
      any->set_type_url(
          "type.googleapis.com/"
          "segmentation_platform.proto.SegmentationModelMetadata");
    }
    DCHECK(model_executor_handle_);

    auto model_metadata = optimization_guide::TestModelInfoBuilder()
                              .SetModelMetadata(any)
                              .SetModelFilePath(model_file_path_)
                              .Build();
    model_executor_handle_->OnModelUpdated(kOptimizationTarget,
                                           *model_metadata);
    RunUntilIdle();
  }

  SegmentationModelHandler* model_executor_handle() {
    return model_executor_handle_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  base::FilePath model_file_path_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;

  std::unique_ptr<SegmentationModelHandler> model_executor_handle_;
};

TEST_F(SegmentationModelExecutorTest, ExecuteWithLoadedModel) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_bucket_duration(42);

  std::unique_ptr<base::RunLoop> model_update_runloop =
      std::make_unique<base::RunLoop>();
  CreateModelExecutor(base::BindRepeating(
      [](base::RunLoop* run_loop,
         proto::SegmentationModelMetadata original_metadata,
         optimization_guide::proto::OptimizationTarget optimization_target,
         proto::SegmentationModelMetadata actual_metadata) {
        // Verify that the callback is invoked with the correct data.
        EXPECT_EQ(kOptimizationTarget, optimization_target);
        EXPECT_TRUE(AreEqual(original_metadata, actual_metadata));
        run_loop->Quit();
      },
      model_update_runloop.get(), metadata));

  // Provide metadata as part of the OnModelUpdated invocation, which will
  // be passed along as a correctly crafted Any proto.
  PushModelFileToModelExecutor(metadata);
  model_update_runloop->Run();

  EXPECT_TRUE(model_executor_handle()->ModelAvailable());

  std::vector<float> input = {4, 5};

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_executor_handle()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop, const absl::optional<float>& output) {
            EXPECT_TRUE(output.has_value());
            // 4 + 5 = 9
            EXPECT_NEAR(9, output.value(), 1e-1);

            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  ResetModelExecutor();
}

TEST_F(SegmentationModelExecutorTest, FailToProvideMetadata) {
  std::unique_ptr<base::RunLoop> model_update_runloop =
      std::make_unique<base::RunLoop>();
  base::MockCallback<SegmentationModelHandler::ModelUpdatedCallback> callback;
  CreateModelExecutor(callback.Get());
  EXPECT_CALL(callback, Run(_, _)).Times(0);

  // Intentionally pass an empty metadata which will pass absl::nullopt as the
  // Any proto.
  PushModelFileToModelExecutor(absl::nullopt);
  model_update_runloop->RunUntilIdle();

  EXPECT_TRUE(model_executor_handle()->ModelAvailable());
}

}  // namespace segmentation_platform
