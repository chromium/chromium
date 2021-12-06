// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_topics_model_executor.h"

#include "base/barrier_closure.h"
#include "base/strings/string_number_conversions.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"

namespace optimization_guide {

namespace {

// The ID of the NONE category in the taxonomy. This node always exists.
// Semantically, the none category is attached to data for which we can say
// with certainty that no single label in the taxonomy is appropriate.
const char kNoneCategoryId[] = "-2";

}  // namespace

PageTopicsModelExecutor::PageTopicsModelExecutor(
    OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const absl::optional<proto::Any>& model_metadata)
    : BertModelHandler(model_provider,
                       background_task_runner,
                       proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
                       model_metadata) {
  SetShouldUnloadModelOnComplete(false);
}
PageTopicsModelExecutor::~PageTopicsModelExecutor() = default;

void PageTopicsModelExecutor::ExecuteOnSingleInput(
    AnnotationType annotation_type,
    const std::string& input,
    base::OnceCallback<void(const BatchAnnotationResult&)> callback) {
  ExecuteModelWithInput(
      base::BindOnce(&PageTopicsModelExecutor::
                         PostprocessCategoriesToBatchAnnotationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     annotation_type, input),
      input);
}

void PageTopicsModelExecutor::PostprocessCategoriesToBatchAnnotationResult(
    base::OnceCallback<void(const BatchAnnotationResult&)> callback,
    AnnotationType annotation_type,
    const std::string& input,
    const absl::optional<std::vector<tflite::task::core::Category>>& output) {
  DCHECK_EQ(annotation_type, AnnotationType::kPageTopics);

  absl::optional<std::vector<WeightedString>> categories;
  if (output) {
    categories = ExtractCategoriesFromModelOutput(*output);
  }
  std::move(callback).Run(
      BatchAnnotationResult::CreatePageTopicsResult(input, categories));
}

absl::optional<std::vector<WeightedString>>
PageTopicsModelExecutor::ExtractCategoriesFromModelOutput(
    const std::vector<tflite::task::core::Category>& model_output) const {
  absl::optional<proto::PageTopicsModelMetadata> model_metadata =
      ParsedSupportedFeaturesForLoadedModel<proto::PageTopicsModelMetadata>();
  if (!model_metadata) {
    return absl::nullopt;
  }

  absl::optional<std::string> visibility_category_name =
      model_metadata->output_postprocessing_params().has_visibility_params() &&
              model_metadata->output_postprocessing_params()
                  .visibility_params()
                  .has_category_name()
          ? absl::make_optional(model_metadata->output_postprocessing_params()
                                    .visibility_params()
                                    .category_name())
          : absl::nullopt;

  std::vector<std::pair<std::string, float>> category_candidates;

  for (const auto& category : model_output) {
    if (visibility_category_name &&
        category.class_name == *visibility_category_name) {
      continue;
    }
    // Assume everything else is for categories.
    int category_id;
    if (base::StringToInt(category.class_name, &category_id)) {
      category_candidates.emplace_back(std::make_pair(
          category.class_name, static_cast<float>(category.score)));
    }
  }

  // Postprocess categories.
  if (!model_metadata->output_postprocessing_params().has_category_params()) {
    // No parameters for postprocessing, so just return.
    return absl::nullopt;
  }
  const proto::PageTopicsCategoryPostprocessingParams category_params =
      model_metadata->output_postprocessing_params().category_params();

  // Determine the categories with the highest weights.
  std::sort(category_candidates.begin(), category_candidates.end(),
            [](const std::pair<std::string, float>& a,
               const std::pair<std::string, float>& b) {
              return a.second > b.second;
            });
  size_t max_categories = static_cast<size_t>(category_params.max_categories());
  float total_weight = 0.0;
  float sum_positive_scores = 0.0;
  absl::optional<std::pair<size_t, float>> none_idx_and_weight;
  std::vector<std::pair<std::string, float>> categories;
  categories.reserve(max_categories);
  for (size_t i = 0; i < category_candidates.size() && i < max_categories;
       i++) {
    std::pair<std::string, float> candidate = category_candidates[i];
    categories.push_back(candidate);
    total_weight += candidate.second;

    if (candidate.second > 0)
      sum_positive_scores += candidate.second;

    if (candidate.first == kNoneCategoryId) {
      none_idx_and_weight = std::make_pair(i, candidate.second);
    }
  }

  // Prune out categories that do not meet the minimum threshold.
  if (category_params.min_category_weight() > 0) {
    categories.erase(
        std::remove_if(categories.begin(), categories.end(),
                       [&](const std::pair<std::string, float>& category) {
                         return category.second <
                                category_params.min_category_weight();
                       }),
        categories.end());
  }

  // Prune out none weights.
  if (total_weight == 0) {
    return absl::nullopt;
  }
  if (none_idx_and_weight) {
    if ((none_idx_and_weight->second / total_weight) >
        category_params.min_none_weight()) {
      // None weight is too strong.
      return absl::nullopt;
    }
    // None weight doesn't matter, so prune it out.
    categories.erase(categories.begin() + none_idx_and_weight->first);
  }

  // Normalize category weights.
  float normalization_factor =
      sum_positive_scores > 0 ? sum_positive_scores : 1.0;
  categories.erase(
      std::remove_if(
          categories.begin(), categories.end(),
          [&](const std::pair<std::string, float>& category) {
            return (category.second / normalization_factor) <
                   category_params.min_normalized_weight_within_top_n();
          }),
      categories.end());

  std::vector<WeightedString> final_categories;
  final_categories.reserve(categories.size());
  for (const auto& category : categories) {
    // We expect the weight to be between 0 and 1.
    DCHECK(category.second >= 0.0 && category.second <= 1.0);
    final_categories.emplace_back(
        WeightedString(category.first, category.second));
  }
  DCHECK_LE(final_categories.size(), max_categories);

  return final_categories;
}

}  // namespace optimization_guide
