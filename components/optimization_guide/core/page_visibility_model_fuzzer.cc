// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/page_visibility_model_executor.h"

class PageVisiblityExecutorFuzzer {
 public:
  PageVisiblityExecutorFuzzer()
      : model_executor_(optimization_guide::PageVisibilityModelExecutor()) {
    model_executor_.InitializeAndMoveToExecutionThread(
        // This is an arbitrarily long time since we don't need to test the
        // timeout behavior here, libfuzzer will take care of hangs.
        /*model_inference_timeout=*/base::Minutes(60),
        optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
        /*execution_task_runner=*/task_environment_.GetMainThreadTaskRunner(),
        /*reply_task_runner=*/task_environment_.GetMainThreadTaskRunner());

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("visibility_test_model.tflite");

    model_executor_.UpdateModelFile(model_file_path);
    model_executor_.SetShouldUnloadModelOnComplete(false);
  }
  ~PageVisiblityExecutorFuzzer() { task_environment_.RunUntilIdle(); }

  optimization_guide::PageVisibilityModelExecutor* executor() {
    return &model_executor_;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  optimization_guide::PageVisibilityModelExecutor model_executor_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static PageVisiblityExecutorFuzzer test;
  static bool had_successful_run = false;

  std::string input(reinterpret_cast<const char*>(data), size);
  test.executor()->SendForExecution(
      base::BindOnce(
          [](const absl::optional<std::vector<tflite::task::core::Category>>&
                 output) {
            if (output && !had_successful_run) {
              had_successful_run = true;
              // Print a single debug message so that its obvious things are
              // working (or able to at least once) when running locally.
              LOG(INFO) << "Congrats! Got a successful model execution. This "
                           "message will not be printed again.";
            }
          }),
      base::TimeTicks(), input);

  // The model executor does some PostTask'ing to manage its state. While these
  // tasks are not important for fuzzing, we don't want to queue up a ton of
  // them.
  test.RunUntilIdle();

  return 0;
}
