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
    const absl::optional<proto::Any>& model_metadata)
    : ModelHandler<tflite::task::processor::EmbeddingResult,
                   const std::string&>(
          model_provider,
          background_task_runner,
          std::make_unique<TextEmbeddingModelExecutor>(),
          /*model_inference_timeout=*/absl::nullopt,
          proto::OPTIMIZATION_TARGET_TEXT_EMBEDDER,
          model_metadata) {}

TextEmbeddingModelHandler::~TextEmbeddingModelHandler() = default;

void TextEmbeddingModelHandler::ExecuteOnSingleInput(
    AnnotationType annotation_type,
    const std::string& input,
    base::OnceCallback<void(const BatchAnnotationResult&)> callback) {
  DCHECK_EQ(annotation_type, AnnotationType::kTextEmbedding);
  ExecuteModelWithInput(
      base::BindOnce(&TextEmbeddingModelHandler::
                         PostprocessEmbeddingsToBatchAnnotationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     annotation_type, input),
      input);
}

void TextEmbeddingModelHandler::PostprocessEmbeddingsToBatchAnnotationResult(
    base::OnceCallback<void(const BatchAnnotationResult&)> callback,
    AnnotationType annotation_type,
    const std::string& input,
    const absl::optional<tflite::task::processor::EmbeddingResult>& output) {
  DCHECK_EQ(annotation_type, AnnotationType::kTextEmbedding);

  absl::optional<std::vector<float>> embedding;
  if (output) {
    embedding = ExtractTextEmbeddingFromModelOutput(*output);
  }
  std::move(callback).Run(
      BatchAnnotationResult::CreateTextEmbeddingResult(input, embedding));
}

absl::optional<std::vector<float>>
TextEmbeddingModelHandler::ExtractTextEmbeddingFromModelOutput(
    const tflite::task::processor::EmbeddingResult& model_output) const {
  if (model_output.embeddings().size() != 1) {
    LOG(ERROR)
        << "Text embedding output did not have exactly 1 embeddings, got: "
        << model_output.embeddings().size();
    return absl::nullopt;
  }
  std::vector<float> embedding_output = {
      model_output.embeddings(0).feature_vector().value_float().begin(),
      model_output.embeddings(0).feature_vector().value_float().end()};
  return embedding_output;
}

}  // namespace optimization_guide
