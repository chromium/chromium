// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/mediapipe_text_model_executor.h"

#include <memory>
#include <string>

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
MediapipeTextModelExecutor::BuildModelExecutionTask(base::File& model_file) {
  const int buffer_size = model_file.GetLength();
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  // SAFETY: buffer_size is the size of the allocation
  size_t read_size =
      UNSAFE_BUFFERS(model_file.Read(0, buffer.get(), buffer_size));
  if (read_size != static_cast<size_t>(buffer_size)) {
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
              // SAFETY: buffer_size is the size of the allocation
              .model_asset_buffer =
                  UNSAFE_BUFFERS(std::make_unique<std::string>(
                      buffer.get(), buffer.get() + buffer_size)),
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
