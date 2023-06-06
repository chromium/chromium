// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/text_embedding_model_handler.h"

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/text_embedding_model_executor.h"

namespace optimization_guide {

TextEmbeddingModelHandler::TextEmbeddingModelHandler(
    OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    proto::OptimizationTarget optimization_target,
    const absl::optional<proto::Any>& model_metadata)
    : ModelHandler<tflite::task::processor::EmbeddingResult,
                   const std::string&>(
          model_provider,
          background_task_runner,
          std::make_unique<TextEmbeddingModelExecutor>(optimization_target),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_target,
          model_metadata) {}

TextEmbeddingModelHandler::~TextEmbeddingModelHandler() = default;

}  // namespace optimization_guide
