// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/bert_model_executor.h"

#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/nlclassifier/bert_nl_classifier.h"

namespace optimization_guide {

BertModelExecutor::BertModelExecutor(
    proto::OptimizationTarget optimization_target)
    : optimization_target_(optimization_target) {}
BertModelExecutor::~BertModelExecutor() = default;

absl::optional<std::vector<tflite::task::core::Category>>
BertModelExecutor::Execute(ModelExecutionTask* execution_task,
                           ExecutionStatus* out_status,
                           const std::string& input) {
  if (input.empty()) {
    *out_status = ExecutionStatus::kErrorEmptyOrInvalidInput;
    return absl::nullopt;
  }
  TRACE_EVENT2("browser", "BertModelExecutor::Execute", "optimization_target",
               GetStringNameForOptimizationTarget(optimization_target_),
               "input_length", input.size());
  *out_status = ExecutionStatus::kSuccess;
  return static_cast<tflite::task::text::nlclassifier::BertNLClassifier*>(
             execution_task)
      ->Classify(input);
}

std::unique_ptr<BertModelExecutor::ModelExecutionTask>
BertModelExecutor::BuildModelExecutionTask(base::MemoryMappedFile* model_file,
                                           ExecutionStatus* out_status) {
  auto maybe_nl_classifier =
      tflite::task::text::nlclassifier::BertNLClassifier::CreateFromBuffer(
          reinterpret_cast<const char*>(model_file->data()),
          model_file->length(), std::make_unique<TFLiteOpResolver>());
  if (maybe_nl_classifier.ok())
    return std::move(maybe_nl_classifier.value());
  *out_status = ExecutionStatus::kErrorModelFileNotValid;
  DLOG(ERROR) << "Unable to load BERT model: "
              << maybe_nl_classifier.status().ToString();
  return nullptr;
}

}  // namespace optimization_guide
