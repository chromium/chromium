// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_visibility_model_executor.h"

#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "components/page_content_annotations/core/page_visibility_op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/proto/nl_classifier_options.pb.h"

namespace page_content_annotations {

PageVisibilityModelExecutor::PageVisibilityModelExecutor()
    : num_threads_(
          optimization_guide::features::OverrideNumThreadsForOptTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY)
              .value_or(-1)) {}
PageVisibilityModelExecutor::~PageVisibilityModelExecutor() = default;

std::optional<std::vector<tflite::task::core::Category>>
PageVisibilityModelExecutor::Execute(
    ModelExecutionTask* execution_task,
    optimization_guide::ExecutionStatus* out_status,
    const std::string& input) {
  if (input.empty()) {
    *out_status =
        optimization_guide::ExecutionStatus::kErrorEmptyOrInvalidInput;
    return std::nullopt;
  }
  TRACE_EVENT2(
      "browser", "PageVisibilityModelExecutor::Execute", "optimization_target",
      optimization_guide::GetStringNameForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY),
      "input_length", input.size());

  auto status_or_result =
      static_cast<tflite::task::text::nlclassifier::NLClassifier*>(
          execution_task)
          ->ClassifyText(input);
  if (absl::IsCancelled(status_or_result.status())) {
    *out_status = optimization_guide::ExecutionStatus::kErrorCancelled;
    return std::nullopt;
  }
  if (!status_or_result.ok()) {
    *out_status = optimization_guide::ExecutionStatus::kErrorUnknown;
    return std::nullopt;
  }
  *out_status = optimization_guide::ExecutionStatus::kSuccess;
  return *status_or_result;
}

base::expected<std::unique_ptr<PageVisibilityModelExecutor::ModelExecutionTask>,
               optimization_guide::ExecutionStatus>
PageVisibilityModelExecutor::BuildModelExecutionTask(
    base::MemoryMappedFile* model_file) {
  tflite::task::text::NLClassifierOptions options;
  *options.mutable_base_options()
       ->mutable_model_file()
       ->mutable_file_content() = std::string(
      reinterpret_cast<const char*>(model_file->data()), model_file->length());
  options.mutable_base_options()
      ->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(num_threads_);
  options.set_output_score_tensor_name("batched_predictions");
  options.set_output_label_tensor_name("batched_prediction_labels");

  auto maybe_nl_classifier =
      tflite::task::text::nlclassifier::NLClassifier::CreateFromOptions(
          std::move(options), std::make_unique<PageVisibilityOpResolver>());
  if (maybe_nl_classifier.ok()) {
    return std::move(maybe_nl_classifier.value());
  }
  DLOG(ERROR) << "Unable to load NL model: "
              << maybe_nl_classifier.status().ToString();
  return base::unexpected(
      optimization_guide::ExecutionStatus::kErrorModelFileNotValid);
}

}  // namespace page_content_annotations
