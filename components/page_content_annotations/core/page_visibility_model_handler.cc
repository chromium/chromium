// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_visibility_model_handler.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_visibility_model_executor.h"

namespace page_content_annotations {

namespace {

const char kNotSensitiveCategory[] = "NOT-SENSITIVE";

}  // namespace

PageVisibilityModelHandler::PageVisibilityModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const std::optional<optimization_guide::proto::Any>& model_metadata)
    : ModelHandler<std::vector<tflite::task::core::Category>,
                   const std::string&>(
          model_provider,
          background_task_runner,
          std::make_unique<PageVisibilityModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
          model_metadata) {
  // Unloading the model is done via custom logic in the PCAService.
  SetShouldUnloadModelOnComplete(false);
}
PageVisibilityModelHandler::~PageVisibilityModelHandler() = default;

void PageVisibilityModelHandler::ExecuteOnSingleInput(
    AnnotationType annotation_type,
    const std::string& input,
    base::OnceCallback<void(const BatchAnnotationResult&)> callback) {
  ExecuteModelWithInput(
      base::BindOnce(&PageVisibilityModelHandler::
                         PostprocessCategoriesToBatchAnnotationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     annotation_type, input),
      input);
}

void PageVisibilityModelHandler::PostprocessCategoriesToBatchAnnotationResult(
    base::OnceCallback<void(const BatchAnnotationResult&)> callback,
    AnnotationType annotation_type,
    const std::string& input,
    const std::optional<std::vector<tflite::task::core::Category>>& output) {
  DCHECK_EQ(annotation_type, AnnotationType::kContentVisibility);

  std::optional<double> visibility_score;
  if (output) {
    visibility_score = ExtractContentVisibilityFromModelOutput(*output);
  }
  std::move(callback).Run(BatchAnnotationResult::CreateContentVisibilityResult(
      input, visibility_score));
}

std::optional<double>
PageVisibilityModelHandler::ExtractContentVisibilityFromModelOutput(
    const std::vector<tflite::task::core::Category>& model_output) const {
  for (const auto& category : model_output) {
    if (category.class_name == kNotSensitiveCategory) {
      return category.score;
    }
  }
  return std::nullopt;
}

}  // namespace page_content_annotations
