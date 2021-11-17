// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_VISIBILITY_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_VISIBILITY_MODEL_EXECUTOR_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/bert_model_handler.h"
#include "components/optimization_guide/core/page_content_annotation_job.h"
#include "components/optimization_guide/core/page_content_annotation_job_executor.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// A BERT-based mode executor for page visibility annotations.
class PageVisibilityModelExecutor : public PageContentAnnotationJobExecutor,
                                    public BertModelHandler {
 public:
  PageVisibilityModelExecutor(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const absl::optional<proto::Any>& model_metadata);
  ~PageVisibilityModelExecutor() override;

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
      const absl::optional<std::vector<tflite::task::core::Category>>& output);

  // Extracts the visibility score from the output of the model.
  // Public for testing.
  absl::optional<double> ExtractContentVisibilityFromModelOutput(
      const std::vector<tflite::task::core::Category>& model_output) const;

 private:
  base::WeakPtrFactory<PageVisibilityModelExecutor> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_VISIBILITY_MODEL_EXECUTOR_H_
