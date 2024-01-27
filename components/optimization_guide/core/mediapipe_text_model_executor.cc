// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/mediapipe_text_model_executor.h"

#include "components/optimization_guide/core/tflite_op_resolver.h"

namespace optimization_guide {

using ::mediapipe::tasks::text::text_classifier::TextClassifier;
using ::mediapipe::tasks::text::text_classifier::TextClassifierOptions;

MediapipeTextModelExecutor::MediapipeTextModelExecutor() = default;
MediapipeTextModelExecutor::~MediapipeTextModelExecutor() = default;

std::optional<std::vector<Category>> MediapipeTextModelExecutor::Execute(
    TextClassifier* execution_task,
    ExecutionStatus* out_status,
    const std::string& input) {
  if (input.empty()) {
    *out_status = ExecutionStatus::kErrorEmptyOrInvalidInput;
    return std::nullopt;
  }
  TRACE_EVENT2("browser", "MediapipeTextModelExecutor::Execute",
               "optimization_target",
               GetStringNameForOptimizationTarget(
                   proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY),
               "input_length", input.size());

  auto status_or_result = execution_task->Classify(input);
  if (absl::IsCancelled(status_or_result.status())) {
    *out_status = ExecutionStatus::kErrorCancelled;
    LOG(ERROR) << "MediaPipe Execution Cancelled: "
               << status_or_result.status();
    return std::nullopt;
  }
  if (!status_or_result.ok()) {
    *out_status = ExecutionStatus::kErrorUnknown;
    LOG(ERROR) << "MediaPipe Execution Error: " << status_or_result.status();
    return std::nullopt;
  }

  CHECK_EQ(status_or_result->classifications.size(), 1U);

  *out_status = ExecutionStatus::kSuccess;
  return status_or_result->classifications.at(0).categories;
}

base::expected<std::unique_ptr<TextClassifier>, ExecutionStatus>
MediapipeTextModelExecutor::BuildModelExecutionTask(
    base::MemoryMappedFile* model_file) {
  // Use the inline struct ctor to bypass the default op resolver ctor which is
  // not linked.
  TextClassifierOptions options{
      .base_options =
          {
              .model_asset_buffer = std::make_unique<std::string>(
                  reinterpret_cast<const char*>(model_file->data()),
                  model_file->length()),
              .op_resolver = std::make_unique<TFLiteOpResolver>(),
          },
  };

  auto maybe_classifier = TextClassifier::Create(
      std::make_unique<TextClassifierOptions>(std::move(options)));
  if (!maybe_classifier.ok()) {
    LOG(ERROR) << "Failed to load model with MediaPipe: "
               << maybe_classifier.status();
    return base::unexpected(ExecutionStatus::kErrorModelFileNotValid);
  }

  return std::move(*maybe_classifier);
}

}  // namespace optimization_guide
