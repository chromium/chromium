// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_visibility_model_executor.h"

#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"

namespace optimization_guide {

PageVisibilityModelExecutor::PageVisibilityModelExecutor(
    OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const absl::optional<proto::Any>& model_metadata)
    : BertModelHandler(model_provider,
                       background_task_runner,
                       proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
                       model_metadata) {
  SetShouldUnloadModelOnComplete(false);
}
PageVisibilityModelExecutor::~PageVisibilityModelExecutor() = default;

void PageVisibilityModelExecutor::ExecuteOnSingleInput(
    AnnotationType annotation_type,
    const std::string& input,
    base::OnceCallback<void(const BatchAnnotationResult&)> callback) {
  ExecuteModelWithInput(
      base::BindOnce(&PageVisibilityModelExecutor::
                         PostprocessCategoriesToBatchAnnotationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     annotation_type, input),
      input);
}

void PageVisibilityModelExecutor::PostprocessCategoriesToBatchAnnotationResult(
    base::OnceCallback<void(const BatchAnnotationResult&)> callback,
    AnnotationType annotation_type,
    const std::string& input,
    const absl::optional<std::vector<tflite::task::core::Category>>& output) {
  DCHECK_EQ(annotation_type, AnnotationType::kContentVisibility);

  absl::optional<double> visibility_score;
  if (output) {
    visibility_score = ExtractContentVisibilityFromModelOutput(*output);
  }
  std::move(callback).Run(BatchAnnotationResult::CreateContentVisibilityResult(
      input, visibility_score));
}

absl::optional<double>
PageVisibilityModelExecutor::ExtractContentVisibilityFromModelOutput(
    const std::vector<tflite::task::core::Category>& model_output) const {
  absl::optional<proto::PageTopicsModelMetadata> model_metadata =
      ParsedSupportedFeaturesForLoadedModel<proto::PageTopicsModelMetadata>();
  if (!model_metadata) {
    return absl::nullopt;
  }

  if (!model_metadata->output_postprocessing_params().has_visibility_params()) {
    return absl::nullopt;
  }

  if (!model_metadata->output_postprocessing_params()
           .visibility_params()
           .has_category_name()) {
    return absl::nullopt;
  }

  std::string visibility_category_name =
      model_metadata->output_postprocessing_params()
          .visibility_params()
          .category_name();

  for (const auto& category : model_output) {
    if (category.class_name == visibility_category_name) {
      return 1.0 - category.score;
    }
  }

  // -1 is a sentinel value that means the visibility of the page was not
  // evaluated.
  return -1.0;
}

}  // namespace optimization_guide
