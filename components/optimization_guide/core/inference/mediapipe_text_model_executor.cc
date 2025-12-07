// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/inference/mediapipe_text_model_executor.h"

#include <memory>
#include <string>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
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
  TRACE_EVENT("optimization_guide", "MediapipeTextModelExecutor::Execute",
              "target",
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
MediapipeTextModelExecutor::BuildModelExecutionTask(base::File& model_file) {
  const int buffer_size = model_file.GetLength();

  // Handle potential error from `GetLength()`.
  if (buffer_size < 0) {
    LOG(ERROR) << "Failed to read model file";
    return base::unexpected(ExecutionStatus::kErrorModelFileNotValid);
  }

  base::HeapArray<char> buffer = base::HeapArray<char>::Uninit(buffer_size);
  base::span<char> buffer_span = buffer.as_span();

  if (!model_file.ReadAndCheck(0, base::as_writable_bytes(buffer_span))) {
    LOG(ERROR) << "Failed to read model file";
    return base::unexpected(ExecutionStatus::kErrorModelFileNotValid);
  }

  // Use the inline struct ctor to bypass the default op resolver ctor which is
  // not linked.
  TextClassifierOptions options{
      .base_options =
          {
              // TODO(https://crbug.com/413070228): Upstream changes to support
              // passing a file descriptor / file handle rather than buffer, so
              // that mediapipe can memory map the file contents.
              .model_asset_buffer = std::make_unique<std::string>(
                  buffer_span.data(), buffer_span.size()),
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
