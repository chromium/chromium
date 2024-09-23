// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_model_manager.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"

namespace page_content_annotations {

namespace {

base::TaskTraits GetTaskTraits() {
  base::TaskTraits task_traits = {base::MayBlock(),
                                  base::TaskPriority::BEST_EFFORT};
  if (base::FeatureList::IsEnabled(
          features::
              kOptimizationGuideUseContinueOnShutdownForPageContentAnnotations)) {
    task_traits = {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  }
  return task_traits;
}

}  // namespace

PageContentAnnotationsModelManager::PageContentAnnotationsModelManager(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_model_provider)
    : optimization_guide_model_provider_(optimization_guide_model_provider) {}

PageContentAnnotationsModelManager::~PageContentAnnotationsModelManager() =
    default;

void PageContentAnnotationsModelManager::SetUpPageVisibilityModel(
    optimization_guide::OptimizationGuideModelProvider*
        optimization_guide_model_provider) {
  if (!features::PageVisibilityBatchAnnotationsEnabled())
    return;

  if (page_visibility_model_handler_)
    return;

  page_visibility_model_handler_ = std::make_unique<PageVisibilityModelHandler>(
      optimization_guide_model_provider,
      base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits()),
      std::nullopt);
}

void PageContentAnnotationsModelManager::RequestAndNotifyWhenModelAvailable(
    AnnotationType type,
    base::OnceCallback<void(bool)> callback) {
  if (type == AnnotationType::kContentVisibility) {
    // No-op if the executor is already setup.
    SetUpPageVisibilityModel(optimization_guide_model_provider_);

    if (page_visibility_model_handler_) {
      page_visibility_model_handler_->AddOnModelUpdatedCallback(
          base::BindOnce(std::move(callback), true));
      return;
    }
  }

  std::move(callback).Run(false);
}

std::optional<optimization_guide::ModelInfo>
PageContentAnnotationsModelManager::GetModelInfoForType(
    AnnotationType type) const {
  if (type == AnnotationType::kContentVisibility &&
      page_visibility_model_handler_) {
    return page_visibility_model_handler_->GetModelInfo();
  }
  return std::nullopt;
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
    if (page_visibility_model_handler_) {
      page_visibility_model_handler_->UnloadModel();
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

  if (job->type() != AnnotationType::kContentVisibility &&
      page_visibility_model_handler_) {
    page_visibility_model_handler_->UnloadModel();
  }

  if (job->type() == AnnotationType::kContentVisibility) {
    if (!page_visibility_model_handler_) {
      job->FillWithNullOutputs();
      job->OnComplete();
      job.reset();
      std::move(on_job_complete_callback).Run();
      return;
    }
    page_visibility_model_handler_->ExecuteJob(
        std::move(on_job_complete_callback), std::move(job));
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace page_content_annotations
