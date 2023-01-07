// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/bert_model_executor.h"

// This fuzzer relies on two macros so that the test code can be easily reused
// across multiple model inputs. Make sure these are defined by the build rule.
//
// OPTIMIZATION_TARGET should be something like
// OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD.
//
// MODEL_FILE_BASENAME should be something like "model.tflite", quotes included!
// Given model file base names use the OptGuide test data dir in //components.
class BertModelExecutorFuzzer {
 public:
  BertModelExecutorFuzzer()
      : model_executor_(optimization_guide::BertModelExecutor(
            optimization_guide::proto::OPTIMIZATION_TARGET)) {
    model_executor_.InitializeAndMoveToExecutionThread(
        // This is an arbitrarily long time since we don't need to test the
        // timeout behavior here, libfuzzer will take care of hangs.
        /*model_inference_timeout=*/base::Minutes(60),
        optimization_guide::proto::OPTIMIZATION_TARGET,
        /*execution_task_runner=*/task_environment_.GetMainThreadTaskRunner(),
        /*reply_task_runner=*/task_environment_.GetMainThreadTaskRunner());

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                         .AppendASCII("test")
                                         .AppendASCII("data")
                                         .AppendASCII("optimization_guide")
                                         .AppendASCII(MODEL_FILE_BASENAME);

    model_executor_.UpdateModelFile(model_file_path);
    model_executor_.SetShouldUnloadModelOnComplete(false);
  }
  ~BertModelExecutorFuzzer() { task_environment_.RunUntilIdle(); }

  optimization_guide::BertModelExecutor* executor() { return &model_executor_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  optimization_guide::BertModelExecutor model_executor_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BertModelExecutorFuzzer test;
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