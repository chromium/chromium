// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/page_entities_model_executor.h"
#include "components/optimization_guide/optimization_guide_buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#include "components/optimization_guide/core/page_entities_model_executor_impl.h"
#endif

namespace optimization_guide {

namespace {

const char kPageTopicsModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata";

// The ID of the NONE category in the taxonomy. This node always exists.
// Semantically, the none category is attached to data for which we can say
// with certainty that no single label in the taxonomy is appropriate.
const char kNoneCategoryId[] = "-2";

// The max number of page entities that should be output.
// TODO(crbug/1234578): Make the number of entities Finch-able once we
// know how much the model is expected to output.
constexpr size_t kMaxPageEntities = 5;

std::unique_ptr<history::VisitContentModelAnnotations>
GetOrCreateCurrentContentModelAnnotations(
    std::unique_ptr<history::VisitContentModelAnnotations>
        current_annotations) {
  if (current_annotations)
    return current_annotations;
  return std::make_unique<history::VisitContentModelAnnotations>();
}

}  // namespace

PageContentAnnotationsModelManager::PageContentAnnotationsModelManager(
    const std::string& application_locale,
    OptimizationGuideModelProvider* optimization_guide_model_provider)
    : optimization_guide_model_provider_(optimization_guide_model_provider) {
  if (features::ShouldExecutePageVisibilityModelOnPageContent(
          application_locale)) {
    SetUpPageTopicsModel(optimization_guide_model_provider);
    ordered_models_to_execute_.push_back(
        proto::OPTIMIZATION_TARGET_PAGE_TOPICS);
  }
  if (features::ShouldExecutePageEntitiesModelOnPageContent(
          application_locale)) {
    SetUpPageEntitiesModel(optimization_guide_model_provider);
    ordered_models_to_execute_.push_back(
        proto::OPTIMIZATION_TARGET_PAGE_ENTITIES);
  }
}

PageContentAnnotationsModelManager::~PageContentAnnotationsModelManager() =
    default;

void PageContentAnnotationsModelManager::Annotate(
    const std::string& text,
    PageContentAnnotatedCallback callback) {
  ExecuteNextModelOrInvokeCallback(text, /*current_annotations=*/nullptr,
                                   std::move(callback),
                                   /*current_model_index=*/absl::nullopt);
}

void PageContentAnnotationsModelManager::ExecuteNextModelOrInvokeCallback(
    const std::string& text,
    std::unique_ptr<history::VisitContentModelAnnotations> current_annotations,
    PageContentAnnotatedCallback callback,
    absl::optional<size_t> current_model_index) {
  size_t next_model_index_to_execute = current_model_index.value_or(-1) + 1;
  if (next_model_index_to_execute >= ordered_models_to_execute_.size()) {
    // We have run all the models, so run the callback.
    DCHECK(callback);
    std::move(callback).Run(current_annotations
                                ? absl::make_optional(*current_annotations)
                                : absl::nullopt);
    return;
  }

  // Execute the next model.
  proto::OptimizationTarget optimization_target =
      ordered_models_to_execute_.at(next_model_index_to_execute);
  switch (optimization_target) {
    case proto::OPTIMIZATION_TARGET_PAGE_TOPICS:
      ExecutePageTopicsModel(text, std::move(current_annotations),
                             std::move(callback), next_model_index_to_execute);
      break;
    case proto::OPTIMIZATION_TARGET_PAGE_ENTITIES:
      ExecutePageEntitiesModel(text, std::move(current_annotations),
                               std::move(callback),
                               next_model_index_to_execute);
      break;
    default:
      NOTREACHED();
      // NOTREACHED is a no-op in release builds, so just run next model.
      ExecuteNextModelOrInvokeCallback(text, std::move(current_annotations),
                                       std::move(callback),
                                       next_model_index_to_execute);
  }
}

void PageContentAnnotationsModelManager::SetUpPageEntitiesModel(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PageContentAnnotationsModelManager."
      "PageEntitiesModelRequested",
      true);
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  base::TaskTraits task_traits = {base::MayBlock(),
                                  base::TaskPriority::BEST_EFFORT};
  if (base::FeatureList::IsEnabled(
          features::
              kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations)) {
    task_traits = {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(task_traits);

  page_entities_model_executor_ =
      std::make_unique<PageEntitiesModelExecutorImpl>(
          optimization_guide_model_provider, background_task_runner);
#endif
}

void PageContentAnnotationsModelManager::
    OverridePageEntitiesModelExecutorForTesting(
        std::unique_ptr<PageEntitiesModelExecutor>
            page_entities_model_executor) {
  page_entities_model_executor_ = std::move(page_entities_model_executor);
}

void PageContentAnnotationsModelManager::ExecutePageEntitiesModel(
    const std::string& text,
    std::unique_ptr<history::VisitContentModelAnnotations> current_annotations,
    PageContentAnnotatedCallback callback,
    size_t current_model_index) {
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PageContentAnnotationsModelManager."
      "PageEntitiesModelExecutionRequested",
      true);
  if (page_entities_model_executor_) {
    page_entities_model_executor_->HumanReadableExecuteModelWithInput(
        text,
        base::BindOnce(&PageContentAnnotationsModelManager::
                           OnPageEntitiesModelExecutionCompleted,
                       weak_ptr_factory_.GetWeakPtr(), text,
                       std::move(current_annotations), base::TimeTicks::Now(),
                       std::move(callback), current_model_index));
    return;
  }
  OnPageEntitiesModelExecutionCompleted(
      text, std::move(current_annotations), base::TimeTicks::Now(),
      std::move(callback), current_model_index,
      /*output=*/absl::nullopt);
}

void PageContentAnnotationsModelManager::OnPageEntitiesModelExecutionCompleted(
    const std::string& text,
    std::unique_ptr<history::VisitContentModelAnnotations> current_annotations,
    base::TimeTicks execution_start,
    PageContentAnnotatedCallback callback,
    size_t current_model_index,
    const absl::optional<std::vector<ScoredEntityMetadata>>& output) {
  if (output) {
    current_annotations = GetOrCreateCurrentContentModelAnnotations(
        std::move(current_annotations));

    // Determine the entities with the highest weights.
    std::vector<ScoredEntityMetadata> entity_candidates = std::move(*output);
    std::sort(entity_candidates.begin(), entity_candidates.end(),
              [](const ScoredEntityMetadata& a, const ScoredEntityMetadata& b) {
                return a.score > b.score;
              });

    // Add the top entities to the working current annotations.
    for (const auto& entity : entity_candidates) {
      if (current_annotations->entities.size() >= kMaxPageEntities) {
        break;
      }

      // We expect the weight to be between 0 and 1, so that the normalization
      // from 0 to 100 makes sense.
      DCHECK(entity.score >= 0.0 && entity.score <= 1.0);
      current_annotations->entities.emplace_back(
          history::VisitContentModelAnnotations::Category(
              entity.metadata.entity_id, static_cast<int>(100 * entity.score)));
    }
    base::UmaHistogramTimes(
        "OptimizationGuide.PageContentAnnotationsService."
        "PageEntitiesExecutionLatency",
        base::TimeTicks::Now() - execution_start);
  }
  ExecuteNextModelOrInvokeCallback(text, std::move(current_annotations),
                                   std::move(callback), current_model_index);
}

void PageContentAnnotationsModelManager::SetUpPageTopicsModel(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  proto::Any model_metadata;
  model_metadata.set_type_url(kPageTopicsModelMetadataTypeUrl);
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.add_supported_output(
      proto::PAGE_TOPICS_SUPPORTED_OUTPUT_VISIBILITY);
  page_topics_model_metadata.add_supported_output(
      proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
  page_topics_model_metadata.SerializeToString(model_metadata.mutable_value());

  base::TaskTraits task_traits = {base::MayBlock(),
                                  base::TaskPriority::BEST_EFFORT};
  if (base::FeatureList::IsEnabled(
          features::
              kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations)) {
    task_traits = {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  }

  page_topics_model_handler_ = std::make_unique<BertModelHandler>(
      optimization_guide_model_provider,
      base::ThreadPool::CreateSequencedTaskRunner(task_traits),
      proto::OPTIMIZATION_TARGET_PAGE_TOPICS, model_metadata);
}

void PageContentAnnotationsModelManager::SetUpPageTopicsV2Model(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  if (!features::PageTopicsBatchAnnotationsEnabled())
    return;

  if (on_demand_page_topics_model_executor_)
    return;

  on_demand_page_topics_model_executor_ =
      std::make_unique<PageTopicsModelExecutor>(
          optimization_guide_model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
          absl::nullopt);
}

void PageContentAnnotationsModelManager::SetUpPageVisibilityModel(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  if (!features::PageVisibilityBatchAnnotationsEnabled())
    return;

  if (page_visibility_model_executor_)
    return;

  page_visibility_model_executor_ =
      std::make_unique<PageVisibilityModelExecutor>(
          optimization_guide_model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
          absl::nullopt);
}

void PageContentAnnotationsModelManager::ExecutePageTopicsModel(
    const std::string& text,
    std::unique_ptr<history::VisitContentModelAnnotations> current_annotations,
    PageContentAnnotatedCallback callback,
    size_t current_model_index) {
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PageContentAnnotationsModelManager."
      "PageTopicsModelExecutionRequested",
      true);

  bool model_available = page_topics_model_handler_->ModelAvailable();

  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotationsService.ModelAvailable",
      model_available);

  if (!model_available) {
    // TODO(crbug/1177102): Figure out if we want to enqueue it for later if
    // model isn't ready, but if we call this when the model isn't ready, it
    // will just return absl::nullopt for now.
    ExecuteNextModelOrInvokeCallback(text, std::move(current_annotations),
                                     std::move(callback), current_model_index);
    return;
  }

  absl::optional<proto::PageTopicsModelMetadata> model_metadata =
      page_topics_model_handler_->ParsedSupportedFeaturesForLoadedModel<
          proto::PageTopicsModelMetadata>();
  if (!model_metadata) {
    NOTREACHED();
    // Continue to run the callback since NOTREACHED is a no-op in prod.
    ExecuteNextModelOrInvokeCallback(text, std::move(current_annotations),
                                     std::move(callback), current_model_index);
    return;
  }

  bool has_supported_output = false;
  for (const auto supported_output : model_metadata->supported_output()) {
    if (supported_output == proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES ||
        supported_output == proto::PAGE_TOPICS_SUPPORTED_OUTPUT_VISIBILITY) {
      has_supported_output = true;
      break;
    }
  }
  if (!has_supported_output) {
    // TODO(crbug/1177102): Add histogram.
    ExecuteNextModelOrInvokeCallback(text, std::move(current_annotations),
                                     std::move(callback), current_model_index);
    return;
  }

  page_topics_model_handler_->ExecuteModelWithInput(
      base::BindOnce(&PageContentAnnotationsModelManager::
                         OnPageTopicsModelExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), text,
                     std::move(current_annotations), std::move(callback),
                     current_model_index, *model_metadata),
      text);
}

void PageContentAnnotationsModelManager::OnPageTopicsModelExecutionCompleted(
    const std::string& text,
    std::unique_ptr<history::VisitContentModelAnnotations> current_annotations,
    PageContentAnnotatedCallback callback,
    size_t current_model_index,
    const proto::PageTopicsModelMetadata& model_metadata,
    const absl::optional<std::vector<tflite::task::core::Category>>& output) {
  if (output) {
    current_annotations = GetOrCreateCurrentContentModelAnnotations(
        std::move(current_annotations));
    PopulateContentModelAnnotationsFromPageTopicsModelOutput(
        model_metadata, *output, current_annotations.get());
  }

  ExecuteNextModelOrInvokeCallback(text, std::move(current_annotations),
                                   std::move(callback), current_model_index);
}

absl::optional<int64_t>
PageContentAnnotationsModelManager::GetPageTopicsModelVersion() const {
  absl::optional<proto::PageTopicsModelMetadata> model_metadata =
      page_topics_model_handler_->ParsedSupportedFeaturesForLoadedModel<
          proto::PageTopicsModelMetadata>();
  if (model_metadata)
    return model_metadata->version();
  return absl::nullopt;
}

void PageContentAnnotationsModelManager::GetMetadataForEntityId(
    const std::string& entity_id,
    EntityMetadataRetrievedCallback callback) {
  if (page_entities_model_executor_) {
    page_entities_model_executor_->GetMetadataForEntityId(entity_id,
                                                          std::move(callback));
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

void PageContentAnnotationsModelManager::
    PopulateContentModelAnnotationsFromPageTopicsModelOutput(
        const proto::PageTopicsModelMetadata& model_metadata,
        const std::vector<tflite::task::core::Category>& model_output,
        history::VisitContentModelAnnotations* out_content_annotations) const {
  out_content_annotations->page_topics_model_version = model_metadata.version();

  absl::optional<std::string> visibility_category_name;
  if (model_metadata.output_postprocessing_params().has_visibility_params()) {
    visibility_category_name = model_metadata.output_postprocessing_params()
                                   .visibility_params()
                                   .category_name();
  }
  // -1 is a sentinel value that means the visibility of the page was not
  // evaluated.
  out_content_annotations->visibility_score = -1.0;
  std::vector<std::pair<std::string, float>> category_candidates;
  for (const auto& category : model_output) {
    if (visibility_category_name &&
        category.class_name == *visibility_category_name) {
      out_content_annotations->visibility_score =
          static_cast<float>(1 - category.score);
      if (!model_metadata.output_postprocessing_params()
               .has_category_params()) {
        break;
      }
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
  if (!model_metadata.output_postprocessing_params().has_category_params()) {
    // No parameters for postprocessing, so just return.
    return;
  }
  const proto::PageTopicsCategoryPostprocessingParams category_params =
      model_metadata.output_postprocessing_params().category_params();

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
    return;
  }
  if (none_idx_and_weight) {
    if ((none_idx_and_weight->second / total_weight) >
        category_params.min_none_weight()) {
      // None weight is too strong.
      return;
    }
    // None weight doesn't matter, so prune it out. Note that it may have
    // already been removed above if its weight was below the category min.
    categories.erase(
        std::remove_if(categories.begin(), categories.end(),
                       [&](const std::pair<std::string, float>& category) {
                         return category.first == kNoneCategoryId;
                       }),
        categories.end());
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
  out_content_annotations->categories = final_categories;
}

void PageContentAnnotationsModelManager::RequestAndNotifyWhenModelAvailable(
    AnnotationType type,
    base::OnceCallback<void(bool)> callback) {
  if (type == AnnotationType::kPageTopics) {
    // No-op if the executor is already setup.
    SetUpPageTopicsV2Model(optimization_guide_model_provider_);

    if (on_demand_page_topics_model_executor_) {
      on_demand_page_topics_model_executor_->AddOnModelUpdatedCallback(
          base::BindOnce(std::move(callback), true));
      return;
    }
  }

  if (type == AnnotationType::kContentVisibility) {
    // No-op if the executor is already setup.
    SetUpPageVisibilityModel(optimization_guide_model_provider_);

    if (page_visibility_model_executor_) {
      page_visibility_model_executor_->AddOnModelUpdatedCallback(
          base::BindOnce(std::move(callback), true));
      return;
    }
  }

  // TODO(crbug/1278828): Add support for page entities.

  std::move(callback).Run(false);
}

absl::optional<ModelInfo>
PageContentAnnotationsModelManager::GetModelInfoForType(
    AnnotationType type) const {
  if (type == AnnotationType::kPageTopics &&
      on_demand_page_topics_model_executor_) {
    return on_demand_page_topics_model_executor_->GetModelInfo();
  }
  if (type == AnnotationType::kContentVisibility &&
      page_visibility_model_executor_) {
    return page_visibility_model_executor_->GetModelInfo();
  }
  // TODO(crbug/1278828): Add support for page entities.
  return absl::nullopt;
}

void PageContentAnnotationsModelManager::Annotate(
    BatchAnnotationCallback callback,
    const std::vector<std::string>& inputs,
    AnnotationType annotation_type) {
  base::UmaHistogramCounts100(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize." +
          AnnotationTypeToString(annotation_type),
      inputs.size());

  std::unique_ptr<PageContentAnnotationJob> job =
      std::make_unique<PageContentAnnotationJob>(std::move(callback), inputs,
                                                 annotation_type);
  job_queue_.Insert(std::move(job), static_cast<size_t>(JobPriority::kNormal));

  MaybeStartNextAnnotationJob();
}

void PageContentAnnotationsModelManager::OnJobExecutionComplete() {
  DCHECK_EQ(job_state_, JobExecutionState::kRunning);
  job_state_ = JobExecutionState::kComplete;

  MaybeStartNextAnnotationJob();
}

void PageContentAnnotationsModelManager::MaybeStartNextAnnotationJob() {
  if (job_state_ == JobExecutionState::kRunning) {
    return;
  }

  JobQueue::Pointer job_ptr = job_queue_.FirstMax();
  if (job_ptr.is_null()) {
    // There are no more jobs to run, so unload all models.
    if (on_demand_page_topics_model_executor_) {
      on_demand_page_topics_model_executor_->UnloadModel();
    }
    if (page_visibility_model_executor_) {
      page_visibility_model_executor_->UnloadModel();
    }
    return;
  }

  DCHECK(job_state_ == JobExecutionState::kIdle ||
         job_state_ == JobExecutionState::kComplete);
  job_state_ = JobExecutionState::kRunning;

  std::unique_ptr<PageContentAnnotationJob> job = job_queue_.Erase(job_ptr);

  base::OnceClosure on_job_complete_callback = base::BindOnce(
      &PageContentAnnotationsModelManager::OnJobExecutionComplete,
      weak_ptr_factory_.GetWeakPtr());

  // Reset every other model from memory so that there aren't a bunch of models
  // all loaded at the same time.
  if (job->type() != AnnotationType::kPageTopics &&
      on_demand_page_topics_model_executor_) {
    on_demand_page_topics_model_executor_->UnloadModel();
  }
  if (job->type() != AnnotationType::kContentVisibility &&
      page_visibility_model_executor_) {
    page_visibility_model_executor_->UnloadModel();
  }

  if (job->type() == AnnotationType::kPageTopics) {
    if (!on_demand_page_topics_model_executor_) {
      job->FillWithNullOutputs();
      job->OnComplete();
      job.reset();
      std::move(on_job_complete_callback).Run();
      return;
    }
    on_demand_page_topics_model_executor_->ExecuteJob(
        std::move(on_job_complete_callback), std::move(job));
    return;
  }

  if (job->type() == AnnotationType::kContentVisibility) {
    if (!page_visibility_model_executor_) {
      job->FillWithNullOutputs();
      job->OnComplete();
      job.reset();
      std::move(on_job_complete_callback).Run();
      return;
    }
    page_visibility_model_executor_->ExecuteJob(
        std::move(on_job_complete_callback), std::move(job));
    return;
  }

  // TODO(crbug/1278828): Add support for page entities.
  if (job->type() == AnnotationType::kPageEntities) {
    job->FillWithNullOutputs();
    job->OnComplete();
    job.reset();
    std::move(on_job_complete_callback).Run();
    return;
  }
  NOTREACHED();
}

}  // namespace optimization_guide
