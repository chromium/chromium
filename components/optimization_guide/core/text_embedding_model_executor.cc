// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/text_embedding_model_executor.h"

#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/text_embedder.h"

namespace optimization_guide {

TextEmbeddingModelExecutor::TextEmbeddingModelExecutor(
    proto::OptimizationTarget optimization_target)
    : optimization_target_(optimization_target),
      num_threads_(features::OverrideNumThreadsForOptTarget(optimization_target)
                       .value_or(-1)) {}

TextEmbeddingModelExecutor::~TextEmbeddingModelExecutor() = default;

absl::optional<tflite::task::processor::EmbeddingResult>
TextEmbeddingModelExecutor::Execute(ModelExecutionTask* execution_task,
                                    ExecutionStatus* out_status,
                                    const std::string& input) {
  if (input.empty()) {
    *out_status = ExecutionStatus::kErrorEmptyOrInvalidInput;
    return absl::nullopt;
  }
  TRACE_EVENT2("browser", "TextEmbeddingModelExecutor::Execute",
               "optimization_target",
               GetStringNameForOptimizationTarget(optimization_target_),
               "input_length", input.size());

  auto status_or_result =
      static_cast<tflite::task::text::TextEmbedder*>(execution_task)
          ->Embed(input);
  if (absl::IsCancelled(status_or_result.status())) {
    *out_status = ExecutionStatus::kErrorCancelled;
    return absl::nullopt;
  }
  if (!status_or_result.ok()) {
    *out_status = ExecutionStatus::kErrorUnknown;
    return absl::nullopt;
  }
  *out_status = ExecutionStatus::kSuccess;
  return *status_or_result;
}

std::unique_ptr<TextEmbeddingModelExecutor::ModelExecutionTask>
TextEmbeddingModelExecutor::BuildModelExecutionTask(
    base::MemoryMappedFile* model_file,
    ExecutionStatus* out_status) {
  tflite::task::text::TextEmbedderOptions options;
  *options.mutable_base_options()
       ->mutable_model_file()
       ->mutable_file_content() = std::string(
      reinterpret_cast<const char*>(model_file->data()), model_file->length());
  options.mutable_base_options()
      ->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(num_threads_);
  auto maybe_text_embedder =
      tflite::task::text::TextEmbedder::CreateFromOptions(
          std::move(options), std::make_unique<TFLiteOpResolver>());
  if (maybe_text_embedder.ok()) {
    return std::move(maybe_text_embedder.value());
  }
  *out_status = ExecutionStatus::kErrorModelFileNotValid;
  DLOG(ERROR) << "Unable to load Text Embedder model: "
              << maybe_text_embedder.status().ToString();
  return nullptr;
}

}  // namespace optimization_guide
