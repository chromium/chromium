// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEDIAPIPE_TEXT_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEDIAPIPE_TEXT_MODEL_EXECUTOR_H_

// TODO(b/283522287): This file has the same header guard as the one in TFLite
// Support, but the two are not interchangeable nor a super/subset of each
// other. The proper fix would be to have mediapipe upstream rename the
// metadata_schema.fbs file and then move its contents in a differently named
// namespace as well (since there are some duplicate symbols between MediaPipe
// and TFLite Support). Until then, putting this include before any of the
// TFLite Support includes (found inside tflite_model_executor.h) is a decent
// bandaid.
#include "third_party/mediapipe/src/mediapipe/tasks/metadata/metadata_schema_generated.h"

#include "components/optimization_guide/core/tflite_model_executor.h"
#include "third_party/mediapipe/src/mediapipe/tasks/cc/text/text_classifier/text_classifier.h"

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
  std::optional<std::vector<Category>> Execute(
      TextClassifier* execution_task,
      ExecutionStatus* out_status,
      const std::string& input) override;
  base::expected<std::unique_ptr<TextClassifier>, ExecutionStatus>
  BuildModelExecutionTask(base::MemoryMappedFile* model_file) override;

  MediapipeTextModelExecutor(const MediapipeTextModelExecutor&) = delete;
  MediapipeTextModelExecutor& operator=(const MediapipeTextModelExecutor&) =
      delete;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MEDIAPIPE_TEXT_MODEL_EXECUTOR_H_
