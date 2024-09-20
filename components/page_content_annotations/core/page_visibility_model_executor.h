// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_MODEL_EXECUTOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_MODEL_EXECUTOR_H_

#include "components/optimization_guide/core/tflite_model_executor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace page_content_annotations {

// A full implementation of a ModelExecutor that executes the page visibility
// model, which needs some special API calls to the TFLite Task library.
class PageVisibilityModelExecutor
    : public optimization_guide::TFLiteModelExecutor<
          std::vector<tflite::task::core::Category>,
          const std::string&> {
 public:
  PageVisibilityModelExecutor();
  ~PageVisibilityModelExecutor() override;

  using ModelExecutionTask =
      tflite::task::core::BaseTaskApi<std::vector<tflite::task::core::Category>,
                                      const std::string&>;

  // optimization_guide::ModelExecutor:
  std::optional<std::vector<tflite::task::core::Category>> Execute(
      ModelExecutionTask* execution_task,
      optimization_guide::ExecutionStatus* out_status,
      const std::string& input) override;
  base::expected<std::unique_ptr<ModelExecutionTask>,
                 optimization_guide::ExecutionStatus>
  BuildModelExecutionTask(base::MemoryMappedFile* model_file) override;

 private:
  // -1 tells TFLite to use its own default number of threads.
  const int num_threads_ = -1;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_MODEL_EXECUTOR_H_
