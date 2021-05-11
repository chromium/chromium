// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_BERT_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_BERT_MODEL_EXECUTOR_H_

#include "components/optimization_guide/content/browser/optimization_target_model_executor.h"
#include "third_party/tflite-support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace optimization_guide {

// An OptimizationTargetModelExecutor that executes BERT models.
//
// Note that sentencepiece tokenizers are not supported by Chromium's copy of
// the TFLite Support library.
class BertModelExecutor : public OptimizationTargetModelExecutor<
                              std::vector<tflite::task::core::Category>,
                              const std::string&> {
 public:
  BertModelExecutor(OptimizationGuideDecider* decider,
                    proto::OptimizationTarget optimization_target,
                    const base::Optional<proto::Any>& model_metadata,
                    const scoped_refptr<base::SequencedTaskRunner>&
                        model_execution_task_runner);
  ~BertModelExecutor() override;
  BertModelExecutor(const BertModelExecutor&) = delete;
  BertModelExecutor& operator=(const BertModelExecutor&) = delete;

 protected:
  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<std::vector<tflite::task::core::Category>,
                                      const std::string&>;

  // OptimizationTargetModelExecutor:
  base::Optional<std::vector<tflite::task::core::Category>> Execute(
      ModelExecutionTask* execution_task,
      const std::string& input) override;
  std::unique_ptr<ModelExecutionTask> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_BERT_MODEL_EXECUTOR_H_
