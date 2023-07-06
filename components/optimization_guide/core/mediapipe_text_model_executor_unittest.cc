// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/mediapipe/src/mediapipe/tasks/metadata/metadata_schema_generated.h"

#include "components/optimization_guide/core/mediapipe_text_model_executor.h"

#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class MediapipeTextModelExecutorTest : public testing::Test {
 public:
  MediapipeTextModelExecutorTest() = default;
  ~MediapipeTextModelExecutorTest() override = default;

  void SetUp() override {
    executor_ = std::make_unique<MediapipeTextModelExecutor>();
    executor_->InitializeAndMoveToExecutionThread(
        /*model_inference_timeout=*/absl::nullopt,
        proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner());

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("page_topics_128_model.tflite");

    executor_->UpdateModelFile(model_file_path);
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  MediapipeTextModelExecutor* executor() { return executor_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MediapipeTextModelExecutor> executor_;
};

TEST_F(MediapipeTextModelExecutorTest, Execute) {
  base::RunLoop run_loop;
  executor()->SendForExecution(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<Category>>& output) {
            EXPECT_TRUE(output);
            for (const auto& cat : *output) {
              LOG(INFO) << cat.category_name.value_or("(empty)") << ": "
                        << cat.score;
            }
            run_loop->Quit();
          },
          &run_loop),
      base::TimeTicks(), "test");
  run_loop.Run();
}

}  // namespace optimization_guide
