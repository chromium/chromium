// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_HANDLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_HANDLER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_handler.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"

namespace optimization_guide {

// An implementation of a ModelHandler that executes Text Embedding models.
class TextEmbeddingModelHandler
    : public ModelHandler<tflite::task::processor::EmbeddingResult,
                          const std::string&> {
 public:
  TextEmbeddingModelHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      proto::OptimizationTarget optimization_target,
      const absl::optional<proto::Any>& model_metadata);
  ~TextEmbeddingModelHandler() override;

  TextEmbeddingModelHandler(const TextEmbeddingModelHandler&) = delete;
  TextEmbeddingModelHandler& operator=(const TextEmbeddingModelHandler&) =
      delete;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_HANDLER_H_
