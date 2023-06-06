// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_EXECUTOR_H_

#include "components/optimization_guide/core/tflite_model_executor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"

namespace optimization_guide {

// A full implementation of a ModelExecutor that executes Text Embedding models.
class TextEmbeddingModelExecutor
    : public TFLiteModelExecutor<tflite::task::processor::EmbeddingResult,
                                 const std::string&> {
 public:
  explicit TextEmbeddingModelExecutor(
      proto::OptimizationTarget optimization_target);
  ~TextEmbeddingModelExecutor() override;

  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<tflite::task::processor::EmbeddingResult,
                                      const std::string&>;

  // ModelExecutor:
  absl::optional<tflite::task::processor::EmbeddingResult> Execute(
      ModelExecutionTask* execution_task,
      ExecutionStatus* out_status,
      const std::string& input) override;
  std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file,
      ExecutionStatus* out_status) override;

 private:
  const proto::OptimizationTarget optimization_target_;

  // -1 tells TFLite to use its own default number of threads.
  const int num_threads_ = -1;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_EXECUTOR_H_
