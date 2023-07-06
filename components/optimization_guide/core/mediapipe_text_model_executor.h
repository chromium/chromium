// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEDIAPIPE_TEXT_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEDIAPIPE_TEXT_MODEL_EXECUTOR_H_

#include "components/optimization_guide/core/tflite_model_executor.h"
#include "third_party/mediapipe/src/mediapipe/tasks/cc/text/text_classifier/text_classifier.h"
#include "third_party/mediapipe/src/mediapipe/tasks/metadata/metadata_schema_generated.h"

namespace optimization_guide {

using ::mediapipe::tasks::components::containers::Category;
using ::mediapipe::tasks::text::text_classifier::TextClassifier;

class MediapipeTextModelExecutor
    : public TFLiteModelExecutor<std::vector<Category>,
                                 const std::string&,
                                 TextClassifier> {
 public:
  MediapipeTextModelExecutor();
  ~MediapipeTextModelExecutor() override;

  // TFLiteModelExecutor:
  absl::optional<std::vector<Category>> Execute(
      TextClassifier* execution_task,
      ExecutionStatus* out_status,
      const std::string& input) override;
  std::unique_ptr<TextClassifier> BuildModelExecutionTask(
      base::MemoryMappedFile* model_file,
      ExecutionStatus* out_status) override;

  MediapipeTextModelExecutor(const MediapipeTextModelExecutor&) = delete;
  MediapipeTextModelExecutor& operator=(const MediapipeTextModelExecutor&) =
      delete;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEDIAPIPE_TEXT_MODEL_EXECUTOR_H_
