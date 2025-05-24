// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_executor.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/perf/perf_test.h"

namespace autofill {

namespace {

using ModelExecutor = optimization_guide::ModelExecutor<
    FieldClassificationModelEncoder::ModelOutput,
    const FieldClassificationModelEncoder::ModelInput&>;
using TokenId = FieldClassificationModelEncoder::TokenId;

// Performance tests for the FieldClassificationModelExecutor. This measures the
// time required to load an internal or external model and run inference on one
// set of form fields. It also measures the time required to do multiple (25)
// inferences to amortize any setup time. The external model has the same
// architecture as the real model but has random weights, it's checked in at
// //components/test/data/autofill/ml_model/autofill_model-fold-one.tflite. The
// internal model is downloaded from Chrome's model serving infrastructure via a
// DEPS hook (src/tools/download_autofill_ml_model.py), it's skipped on Chromium
// builds.

class FieldClassificationModelExecutorPerfTest : public testing::Test {
 public:
  FieldClassificationModelExecutorPerfTest() = default;
  ~FieldClassificationModelExecutorPerfTest() override = default;

  void RunTestInternal(const base::FilePath& model_path,
                       const std::string& result_tag,
                       const size_t num_inferences = 1);

 protected:
  base::test::TaskEnvironment task_environment_;
};

void FieldClassificationModelExecutorPerfTest::RunTestInternal(
    const base::FilePath& model_path,
    const std::string& result_tag,
    const size_t num_inferences) {
  // Double-check that the model's been packaged up.
  if (!PathExists(model_path)) {
    GTEST_SKIP() << "Model at '" << model_path << "' is unavailable.";
  }

  auto reporter = perf_test::PerfResultReporter("AutofillML.", result_tag);
  auto runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  auto executor = std::make_unique<FieldClassificationModelExecutor>();
  executor->InitializeAndMoveToExecutionThread(
      /*model_inference_timeout=*/std::nullopt,
      optimization_guide::proto::
          OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
      runner, base::SequencedTaskRunner::GetCurrentDefault());
  runner->PostTask(
    FROM_HERE,
    base::BindOnce(&ModelExecutor::UpdateModelFile,
                    executor->GetWeakPtrForExecutionThread(), model_path));
  {
    base::ElapsedTimer inference_timer;
    for (size_t i = 0; i < num_inferences; i++) {
      // Execute the test in exactly the same way as model_executor_unittest.
      FieldClassificationModelEncoder::ModelInput input = {{
                                                               TokenId(14),
                                                               TokenId(1),
                                                               TokenId(2),
                                                               TokenId(3),
                                                               TokenId(4),
                                                               TokenId(5),
                                                               TokenId(2),
                                                               TokenId(3),
                                                               TokenId(4),
                                                               TokenId(5),
                                                               TokenId(1),
                                                               TokenId(3),
                                                               TokenId(4),
                                                               TokenId(5),
                                                               TokenId(5),
                                                               TokenId(5),
                                                           },
                                                           {
                                                               TokenId(14),
                                                               TokenId(1),
                                                               TokenId(3),
                                                               TokenId(4),
                                                               TokenId(5),
                                                               TokenId(6),
                                                               TokenId(3),
                                                               TokenId(4),
                                                               TokenId(5),
                                                               TokenId(6),
                                                               TokenId(2),
                                                               TokenId(4),
                                                               TokenId(5),
                                                               TokenId(6),
                                                               TokenId(6),
                                                               TokenId(6),
                                                           }};
      base::test::TestFuture<
          const std::optional<FieldClassificationModelEncoder::ModelOutput>&>
          predictions;
      runner->PostTask(
          FROM_HERE,
          base::BindOnce(&ModelExecutor::SendForExecution,
                         executor->GetWeakPtrForExecutionThread(),
                         predictions.GetCallback(),
                         /*start_time=*/base::TimeTicks::Now(), input));

      // Make sure that this isn't optimized out.
      ASSERT_TRUE(predictions.Get());
    }
    base::TimeDelta elapsed = inference_timer.Elapsed();
    reporter.RegisterImportantMetric("Time", "us");
    reporter.AddResult("Time", elapsed);
  }
  runner->DeleteSoon(FROM_HERE, std::move(executor));
  task_environment_.RunUntilIdle();
}

static base::FilePath GetExternalModelPath() {
  // This is a randomly-initialized version of the real model: should have the
  // same architecture and use the same operations.
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  auto model_file_path = source_root_dir.AppendASCII("components")
                             .AppendASCII("test")
                             .AppendASCII("data")
                             .AppendASCII("autofill")
                             .AppendASCII("ml_model")
                             .AppendASCII("autofill_model-fold-one.tflite");
  return model_file_path;
}

static base::FilePath GetInternalModelPath() {
  // This is an internal version of the autofill model.
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  auto model_file_path = source_root_dir.AppendASCII("components")
                             .AppendASCII("test")
                             .AppendASCII("data")
                             .AppendASCII("autofill")
                             .AppendASCII("ml_model")
                             .AppendASCII("internal")
                             .AppendASCII("model.tflite");
  return model_file_path;
}

TEST_F(FieldClassificationModelExecutorPerfTest, TimeToFirstInferenceExternal) {
  // Test covers how long it takes to load the model and run one single
  // inference.
  RunTestInternal(GetExternalModelPath(), "TimeToFirstInference_EXTERNAL");
}

TEST_F(FieldClassificationModelExecutorPerfTest, TimeToFirstInferenceInternal) {
  // Test covers how long it takes to load the model and run one single
  // inference.
  RunTestInternal(GetInternalModelPath(), "TimeToFirstInference_INTERNAL");
}

TEST_F(FieldClassificationModelExecutorPerfTest, TimeFor25InferencesExternal) {
  // Test covers how long it takes to do multiple, successive inferences (e.g.
  // if a form is updating).
  RunTestInternal(GetExternalModelPath(), "TimeFor25Inferences_EXTERNAL", 25);
}

TEST_F(FieldClassificationModelExecutorPerfTest, TimeFor25InferencesInternal) {
  // Test covers how long it takes to do multiple, successive inferences (e.g.
  // if a form is updating).
  RunTestInternal(GetInternalModelPath(), "TimeFor25Inferences_INTERNAL", 25);
}

}  // namespace

}  // namespace autofill
