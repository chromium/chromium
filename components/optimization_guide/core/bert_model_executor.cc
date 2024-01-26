// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/bert_model_executor.h"

#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/bert_nl_classifier.h"

namespace optimization_guide {

BertModelExecutor::BertModelExecutor(
    proto::OptimizationTarget optimization_target)
    : optimization_target_(optimization_target),
      num_threads_(features::OverrideNumThreadsForOptTarget(optimization_target)
                       .value_or(-1)) {}
BertModelExecutor::~BertModelExecutor() = default;

std::optional<std::vector<tflite::task::core::Category>>
BertModelExecutor::Execute(ModelExecutionTask* execution_task,
                           ExecutionStatus* out_status,
                           const std::string& input) {
  if (input.empty()) {
    *out_status = ExecutionStatus::kErrorEmptyOrInvalidInput;
    return std::nullopt;
  }
  TRACE_EVENT2("browser", "BertModelExecutor::Execute", "optimization_target",
               GetStringNameForOptimizationTarget(optimization_target_),
               "input_length", input.size());

  auto status_or_result =
      static_cast<tflite::task::text::BertNLClassifier*>(execution_task)
          ->ClassifyText(input);
  if (absl::IsCancelled(status_or_result.status())) {
    *out_status = ExecutionStatus::kErrorCancelled;
    return std::nullopt;
  }
  if (!status_or_result.ok()) {
    *out_status = ExecutionStatus::kErrorUnknown;
    return std::nullopt;
  }
  *out_status = ExecutionStatus::kSuccess;
  return *status_or_result;
}

base::expected<std::unique_ptr<BertModelExecutor::ModelExecutionTask>,
               ExecutionStatus>
BertModelExecutor::BuildModelExecutionTask(base::MemoryMappedFile* model_file) {
  tflite::task::text::BertNLClassifierOptions options;
  *options.mutable_base_options()
       ->mutable_model_file()
       ->mutable_file_content() = std::string(
      reinterpret_cast<const char*>(model_file->data()), model_file->length());
  options.mutable_base_options()
      ->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(num_threads_);
  auto maybe_nl_classifier =
      tflite::task::text::BertNLClassifier::CreateFromOptions(
          std::move(options), std::make_unique<TFLiteOpResolver>());
  if (maybe_nl_classifier.ok())
    return std::move(maybe_nl_classifier.value());
  DLOG(ERROR) << "Unable to load BERT model: "
              << maybe_nl_classifier.status().ToString();
  return base::unexpected(ExecutionStatus::kErrorModelFileNotValid);
}

}  // namespace optimization_guide
