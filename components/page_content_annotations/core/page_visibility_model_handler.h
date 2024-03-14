// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_MODEL_HANDLER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_MODEL_HANDLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/page_content_annotations/core/page_content_annotation_job.h"
#include "components/page_content_annotations/core/page_content_annotation_job_executor.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace page_content_annotations {

// A NL-based model handler for page visibility annotations.
class PageVisibilityModelHandler
    : public PageContentAnnotationJobExecutor,
      public optimization_guide::ModelHandler<
          std::vector<tflite::task::core::Category>,
          const std::string&> {
 public:
  PageVisibilityModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::optional<optimization_guide::proto::Any>& model_metadata);
  ~PageVisibilityModelHandler() override;

  // PageContentAnnotationJobExecutor:
  void ExecuteOnSingleInput(
      AnnotationType annotation_type,
      const std::string& input,
      base::OnceCallback<void(const BatchAnnotationResult&)> callback) override;

  // Creates a BatchAnnotationResult from the output of the model, calling
  // |ExtractContentVisibilityFromModelOutput| in the process.
  // Public for testing.
  void PostprocessCategoriesToBatchAnnotationResult(
      base::OnceCallback<void(const BatchAnnotationResult&)> callback,
      AnnotationType annotation_type,
      const std::string& input,
      const std::optional<std::vector<tflite::task::core::Category>>& output);

  // Extracts the visibility score from the output of the model, 0 is less
  // visible, 1 is more visible. Public for testing.
  std::optional<double> ExtractContentVisibilityFromModelOutput(
      const std::vector<tflite::task::core::Category>& model_output) const;

 private:
  base::WeakPtrFactory<PageVisibilityModelHandler> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_MODEL_HANDLER_H_
