// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/optimization_guide/segmentation_model_executor.h"

#include <memory>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_handler.h"
#include "components/segmentation_platform/internal/execution/optimization_guide/optimization_guide_segmentation_model_provider.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/segment_id_convertor.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace segmentation_platform {
namespace {
const auto kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
const int64_t kModelVersion = 123;
}  // namespace

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
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("segmentation_platform")
                           .AppendASCII("adder.tflite");

    optimization_guide_segmentation_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
  }

  void TearDown() override { ResetModelExecutor(); }

  void CreateModelExecutor(ModelProvider::ModelUpdatedCallback callback) {
    if (opt_guide_model_provider_)
      opt_guide_model_provider_.reset();

    opt_guide_model_provider_ =
        std::make_unique<OptimizationGuideSegmentationModelProvider>(
            optimization_guide_segmentation_model_provider_.get(),
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
            kSegmentId);
    opt_guide_model_provider_->InitAndFetchModel(callback);
  }

  void ResetModelExecutor() {
    opt_guide_model_provider_.reset();
    // Allow for the SegmentationModelExecutor owned by ModelProvider
    // to be destroyed.
    RunUntilIdle();
  }

  void PushModelFileToModelExecutor(
      std::optional<proto::SegmentationModelMetadata> metadata) {
    std::optional<optimization_guide::proto::Any> any;

    // Craft a correct Any proto in the case we passed in metadata.
    if (metadata.has_value()) {
      std::string serialized_metadata;
      (*metadata).SerializeToString(&serialized_metadata);
      optimization_guide::proto::Any any_proto;
      any = std::make_optional(any_proto);
      any->set_value(serialized_metadata);
      // Need to set the type URL for ParsedSupportedFeaturesForLoadedModel() to
      // work correctly, since it's verifying the type name.
      any->set_type_url(
          "type.googleapis.com/"
          "segmentation_platform.proto.SegmentationModelMetadata");
    }
    DCHECK(opt_guide_model_provider_);

    auto model_metadata = optimization_guide::TestModelInfoBuilder()
                              .SetModelMetadata(any)
                              .SetModelFilePath(model_file_path_)
                              .SetVersion(kModelVersion)
                              .Build();
    opt_guide_model_handler().OnModelUpdated(
        *SegmentIdToOptimizationTarget(kSegmentId), *model_metadata);
    RunUntilIdle();
  }

  OptimizationGuideSegmentationModelHandler& opt_guide_model_handler() {
    return opt_guide_model_provider_->model_handler_for_testing();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::FilePath model_file_path_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_segmentation_model_provider_;

  std::unique_ptr<OptimizationGuideSegmentationModelProvider>
      opt_guide_model_provider_;
};

TEST_F(SegmentationModelExecutorTest, ExecuteWithLoadedModel) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_bucket_duration(42);

  std::unique_ptr<base::RunLoop> model_update_runloop =
      std::make_unique<base::RunLoop>();
  CreateModelExecutor(base::BindRepeating(
      [](base::RunLoop* run_loop,
         proto::SegmentationModelMetadata original_metadata,
         proto::SegmentId segment_id,
         std::optional<proto::SegmentationModelMetadata> actual_metadata,
         int64_t model_version) {
        // Verify that the callback is invoked with the correct data.
        EXPECT_EQ(kSegmentId, segment_id);
        EXPECT_TRUE(AreEqual(original_metadata, actual_metadata.value()));
        EXPECT_EQ(kModelVersion, model_version);
        run_loop->Quit();
      },
      model_update_runloop.get(), metadata));

  // Provide metadata as part of the OnModelUpdated invocation, which will
  // be passed along as a correctly crafted Any proto.
  PushModelFileToModelExecutor(metadata);
  model_update_runloop->Run();

  EXPECT_TRUE(opt_guide_model_handler().ModelAvailable());

  ModelProvider::Request input = {4, 5};

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  opt_guide_model_provider_->ExecuteModelWithInput(
      input, base::BindOnce(
                 [](base::RunLoop* run_loop,
                    const std::optional<ModelProvider::Response>& output) {
                   EXPECT_TRUE(output.has_value());
                   // 4 + 5 = 9
                   EXPECT_NEAR(9, output.value().at(0), 1e-1);

                   run_loop->Quit();
                 },
                 run_loop.get()));
  run_loop->Run();

  ResetModelExecutor();
}

TEST_F(SegmentationModelExecutorTest, FailToProvideMetadata) {
  std::unique_ptr<base::RunLoop> model_update_runloop =
      std::make_unique<base::RunLoop>();
  base::MockCallback<ModelProvider::ModelUpdatedCallback> callback;
  CreateModelExecutor(callback.Get());
  EXPECT_CALL(callback, Run(_, _, _)).Times(0);

  // Intentionally pass an empty metadata which will pass std::nullopt as the
  // Any proto.
  PushModelFileToModelExecutor(std::nullopt);
  model_update_runloop->RunUntilIdle();

  EXPECT_TRUE(opt_guide_model_handler().ModelAvailable());
}

}  // namespace segmentation_platform
