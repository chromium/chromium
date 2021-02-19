// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/bert_model_executor.h"

#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/text/nlclassifier/bert_nl_classifier.h"

namespace optimization_guide {

BertModelExecutor::BertModelExecutor(
    OptimizationGuideDecider* decider,
    proto::OptimizationTarget optimization_target,
    const base::Optional<proto::Any>& model_metadata,
    const scoped_refptr<base::SequencedTaskRunner>& model_execution_task_runner)
    : OptimizationTargetModelExecutor<std::vector<tflite::task::core::Category>,
                                      const std::string&>(
          decider,
          optimization_target,
          model_metadata,
          model_execution_task_runner) {}

BertModelExecutor::~BertModelExecutor() = default;

base::Optional<std::vector<tflite::task::core::Category>>
BertModelExecutor::Execute(
    BertModelExecutor::ModelExecutionTask* execution_task,
    const std::string& input) {
  return static_cast<tflite::task::text::nlclassifier::BertNLClassifier*>(
             execution_task)
      ->Classify(input);
}

std::unique_ptr<BertModelExecutor::ModelExecutionTask>
BertModelExecutor::BuildModelExecutionTask(base::MemoryMappedFile* model_file) {
  auto maybe_nl_classifier =
      tflite::task::text::nlclassifier::BertNLClassifier::CreateFromBuffer(
          reinterpret_cast<const char*>(model_file->data()),
          model_file->length(), std::make_unique<TFLiteOpResolver>());
  if (maybe_nl_classifier.ok())
    return std::move(maybe_nl_classifier.value());
  DLOG(ERROR) << "Unable to load BERT model: "
              << maybe_nl_classifier.status().ToString();
  return nullptr;
}

}  // namespace optimization_guide
