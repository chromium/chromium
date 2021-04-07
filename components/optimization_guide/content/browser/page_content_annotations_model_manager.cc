// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"

#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"

namespace optimization_guide {

namespace {

const char kPageTopicsModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata";

// The ID of the NONE category in the taxonomy. This node always exists.
// Semantically, the none category is attached to data for which we can say
// with certainty that no single label in the taxonomy is appropriate.
const int kNoneCategoryId = -2;

}  // namespace

PageContentAnnotationsModelManager::PageContentAnnotationsModelManager(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider) {
  proto::Any model_metadata;
  model_metadata.set_type_url(kPageTopicsModelMetadataTypeUrl);
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.add_supported_output(
      proto::PAGE_TOPICS_SUPPORTED_OUTPUT_FLOC_PROTECTED);
  page_topics_model_metadata.add_supported_output(
      proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
  page_topics_model_metadata.SerializeToString(model_metadata.mutable_value());

  page_topics_model_executor_ = std::make_unique<BertModelExecutor>(
      optimization_guide_decider, proto::OPTIMIZATION_TARGET_PAGE_TOPICS,
      model_metadata,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

PageContentAnnotationsModelManager::~PageContentAnnotationsModelManager() =
    default;

void PageContentAnnotationsModelManager::Annotate(
    const std::string& text,
    PageContentAnnotatedCallback callback) {
  base::Optional<proto::PageTopicsModelMetadata> model_metadata =
      page_topics_model_executor_->ParsedSupportedFeaturesForLoadedModel<
          proto::PageTopicsModelMetadata>();
  if (!model_metadata) {
    // TODO(crbug/1177102): Figure out if we want to enqueue it for later if
    // model isn't ready, but if we call this when the model isn't ready, it
    // will just return base::nullopt for now.
    return;
  }
  bool has_supported_output = false;
  for (const auto supported_output : model_metadata->supported_output()) {
    if (supported_output == proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES ||
        supported_output ==
            proto::PAGE_TOPICS_SUPPORTED_OUTPUT_FLOC_PROTECTED) {
      has_supported_output = true;
      break;
    }
  }
  if (!has_supported_output) {
    // TODO(crbug/1177102): Add histogram.
    return;
  }
  page_topics_model_executor_->ExecuteModelWithInput(
      base::BindOnce(&PageContentAnnotationsModelManager::
                         OnPageTopicsModelExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     *model_metadata),
      text);
}

void PageContentAnnotationsModelManager::OnPageTopicsModelExecutionCompleted(
    PageContentAnnotatedCallback callback,
    const proto::PageTopicsModelMetadata& model_metadata,
    const base::Optional<std::vector<tflite::task::core::Category>>& output) {
  base::Optional<history::VisitContentModelAnnotations> content_annotations;
  if (output) {
    content_annotations =
        GetContentModelAnnotationsFromOutput(model_metadata, *output);
  }
  std::move(callback).Run(content_annotations);
}

base::Optional<int64_t>
PageContentAnnotationsModelManager::GetPageTopicsModelVersion() const {
  base::Optional<proto::PageTopicsModelMetadata> model_metadata =
      page_topics_model_executor_->ParsedSupportedFeaturesForLoadedModel<
          proto::PageTopicsModelMetadata>();
  if (model_metadata)
    return model_metadata->version();
  return base::nullopt;
}

history::VisitContentModelAnnotations
PageContentAnnotationsModelManager::GetContentModelAnnotationsFromOutput(
    const proto::PageTopicsModelMetadata& model_metadata,
    const std::vector<tflite::task::core::Category>& model_output) const {
  base::Optional<std::string> floc_protected_category_name;
  if (model_metadata.output_postprocessing_params()
          .has_floc_protected_params()) {
    floc_protected_category_name = model_metadata.output_postprocessing_params()
                                       .floc_protected_params()
                                       .category_name();
  }
  float floc_protected_score = -1.0;
  std::vector<std::pair<int, float>> category_candidates;
  for (const auto& category : model_output) {
    if (floc_protected_category_name &&
        category.class_name == *floc_protected_category_name) {
      floc_protected_score = static_cast<float>(category.score);
      if (!model_metadata.output_postprocessing_params()
               .has_category_params()) {
        break;
      }
      continue;
    }

    // Assume everything else is for categories.
    int category_id;
    if (base::StringToInt(category.class_name, &category_id)) {
      category_candidates.emplace_back(
          std::make_pair(category_id, static_cast<float>(category.score)));
    }
  }

  // Postprocess categories.
  if (!model_metadata.output_postprocessing_params().has_category_params()) {
    // No parameters for postprocessing, so just return.
    return history::VisitContentModelAnnotations(
        floc_protected_score, /*categories=*/{}, model_metadata.version());
  }
  const proto::PageTopicsCategoryPostprocessingParams category_params =
      model_metadata.output_postprocessing_params().category_params();

  // Determine the categories with the highest weights.
  std::sort(category_candidates.begin(), category_candidates.end(),
            [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
              return a.second > b.second;
            });
  size_t max_categories = static_cast<size_t>(category_params.max_categories());
  float total_weight = 0.0;
  float sum_positive_scores = 0.0;
  base::Optional<std::pair<size_t, float>> none_idx_and_weight;
  std::vector<std::pair<int, float>> categories;
  categories.reserve(max_categories);
  for (size_t i = 0; i < category_candidates.size() && i < max_categories;
       i++) {
    std::pair<int, float> candidate = category_candidates[i];
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
                       [&](const std::pair<int, float>& category) {
                         return category.second <
                                category_params.min_category_weight();
                       }),
        categories.end());
  }

  // Prune out none weights.
  if (total_weight == 0) {
    return history::VisitContentModelAnnotations(
        floc_protected_score, /*categories=*/{}, model_metadata.version());
  }
  if (none_idx_and_weight) {
    if ((none_idx_and_weight->second / total_weight) >
        category_params.min_none_weight()) {
      // None weight is too strong -
      return history::VisitContentModelAnnotations(
          floc_protected_score, /*categories=*/{}, model_metadata.version());
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
          [&](const std::pair<int, float>& category) {
            return (category.second / normalization_factor) <
                   category_params.min_normalized_weight_within_top_n();
          }),
      categories.end());

  std::vector<history::VisitContentModelAnnotations::Category> final_categories;
  final_categories.reserve(categories.size());
  for (const auto& category : categories) {
    // We expect the weight to be between 0 and 1, so that the normalization
    // from 0 to 100 makes sense.
    DCHECK(category.second >= 0.0 && category.second <= 1.0);
    final_categories.emplace_back(
        history::VisitContentModelAnnotations::Category(
            category.first, static_cast<int>(100 * category.second)));
  }
  DCHECK_LE(final_categories.size(), max_categories);
  return history::VisitContentModelAnnotations(
      floc_protected_score, final_categories, model_metadata.version());
}

}  // namespace optimization_guide
