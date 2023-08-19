// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_HANDLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_HANDLER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/page_content_annotation_job.h"
#include "components/optimization_guide/core/page_content_annotation_job_executor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/processor/proto/embedding.pb.h"

namespace optimization_guide {

// An implementation of a ModelHandler that executes Text Embedding models.
class TextEmbeddingModelHandler
    : public PageContentAnnotationJobExecutor,
      public ModelHandler<tflite::task::processor::EmbeddingResult,
                          const std::string&> {
 public:
  TextEmbeddingModelHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const absl::optional<proto::Any>& model_metadata);
  ~TextEmbeddingModelHandler() override;

  TextEmbeddingModelHandler(const TextEmbeddingModelHandler&) = delete;
  TextEmbeddingModelHandler& operator=(const TextEmbeddingModelHandler&) =
      delete;

  // PageContentAnnotationJobExecutor:
  void ExecuteOnSingleInput(
      AnnotationType annotation_type,
      const std::string& input,
      base::OnceCallback<void(const BatchAnnotationResult&)> callback) override;

  // Creates a BatchAnnotationResult from the output of the model, calling
  // |ExtractTextEmbeddingFromModelOutput| in the process.
  void PostprocessEmbeddingsToBatchAnnotationResult(
      base::OnceCallback<void(const BatchAnnotationResult&)> callback,
      AnnotationType annotation_type,
      const std::string& input,
      const absl::optional<tflite::task::processor::EmbeddingResult>& output);

  // Extracts the vector of floats from the output of the model.
  absl::optional<std::vector<float>> ExtractTextEmbeddingFromModelOutput(
      const tflite::task::processor::EmbeddingResult& model_output) const;

 private:
  base::WeakPtrFactory<TextEmbeddingModelHandler> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEXT_EMBEDDING_MODEL_HANDLER_H_
