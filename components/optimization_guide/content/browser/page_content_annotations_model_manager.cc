// Copyright 2021 The Chromium Authors
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
#include "components/optimization_guide/core/page_entities_model_handler.h"
#include "components/optimization_guide/optimization_guide_buildflags.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#include "components/optimization_guide/core/page_entities_model_handler_impl.h"
#endif

namespace optimization_guide {

namespace {

const char kPageTopicsModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata";

// The current version the client supports for the topics model. This
// should be incremented any time there is a client code change to how the
// topics model works that needs to be side-channeled to the server.
extern const int32_t kTopicsModelVersion = 2;

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
    OptimizationGuideModelProvider* optimization_guide_model_provider)
    : optimization_guide_model_provider_(optimization_guide_model_provider) {}

PageContentAnnotationsModelManager::~PageContentAnnotationsModelManager() =
    default;

void PageContentAnnotationsModelManager::SetUpPageEntitiesModel(
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    base::OnceCallback<void(bool)> callback) {
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PageContentAnnotationsModelManager."
      "PageEntitiesModelRequested",
      true);
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits());

  page_entities_model_handler_ = std::make_unique<PageEntitiesModelHandlerImpl>(
      optimization_guide_model_provider, background_task_runner);

  page_entities_model_handler_->AddOnModelUpdatedCallback(
      base::BindOnce(std::move(callback), true));
#else
  std::move(callback).Run(false);
#endif
}

void PageContentAnnotationsModelManager::
    OverridePageEntitiesModelHandlerForTesting(
        std::unique_ptr<PageEntitiesModelHandler> page_entities_model_handler) {
  page_entities_model_handler_ = std::move(page_entities_model_handler);
}

void PageContentAnnotationsModelManager::SetUpPageTopicsV2Model(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  if (!features::PageTopicsBatchAnnotationsEnabled())
    return;

  if (page_topics_model_handler_)
    return;

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(kPageTopicsModelMetadataTypeUrl);
  proto::PageTopicsModelMetadata model_metadata;
  model_metadata.set_version(kTopicsModelVersion);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  page_topics_model_handler_ = std::make_unique<PageTopicsModelHandler>(
      optimization_guide_model_provider,
      base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits()),
      any_metadata);
}

void PageContentAnnotationsModelManager::SetUpPageVisibilityModel(
    OptimizationGuideModelProvider* optimization_guide_model_provider) {
  if (!features::PageVisibilityBatchAnnotationsEnabled())
    return;

  if (page_visibility_model_handler_)
    return;

  page_visibility_model_handler_ = std::make_unique<PageVisibilityModelHandler>(
      optimization_guide_model_provider,
      base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits()),
      absl::nullopt);
}

void PageContentAnnotationsModelManager::GetMetadataForEntityId(
    const std::string& entity_id,
    EntityMetadataRetrievedCallback callback) {
  if (page_entities_model_handler_) {
    page_entities_model_handler_->GetMetadataForEntityId(entity_id,
                                                         std::move(callback));
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

void PageContentAnnotationsModelManager::RequestAndNotifyWhenModelAvailable(
    AnnotationType type,
    base::OnceCallback<void(bool)> callback) {
  if (type == AnnotationType::kPageTopics) {
    // No-op if the executor is already setup.
    SetUpPageTopicsV2Model(optimization_guide_model_provider_);

    if (page_topics_model_handler_) {
      page_topics_model_handler_->AddOnModelUpdatedCallback(
          base::BindOnce(std::move(callback), true));
      return;
    }
  }

  if (type == AnnotationType::kContentVisibility) {
    // No-op if the executor is already setup.
    SetUpPageVisibilityModel(optimization_guide_model_provider_);

    if (page_visibility_model_handler_) {
      page_visibility_model_handler_->AddOnModelUpdatedCallback(
          base::BindOnce(std::move(callback), true));
      return;
    }
  }

  if (type == AnnotationType::kPageEntities) {
    if (page_entities_model_handler_) {
      page_entities_model_handler_->AddOnModelUpdatedCallback(
          base::BindOnce(std::move(callback), true));
    } else {
      SetUpPageEntitiesModel(optimization_guide_model_provider_,
                             std::move(callback));
    }
    return;
  }

  std::move(callback).Run(false);
}

absl::optional<ModelInfo>
PageContentAnnotationsModelManager::GetModelInfoForType(
    AnnotationType type) const {
  if (type == AnnotationType::kPageTopics && page_topics_model_handler_) {
    return page_topics_model_handler_->GetModelInfo();
  }
  if (type == AnnotationType::kContentVisibility &&
      page_visibility_model_handler_) {
    return page_visibility_model_handler_->GetModelInfo();
  }
  if (type == AnnotationType::kPageEntities && page_entities_model_handler_) {
    return page_entities_model_handler_->GetModelInfo();
  }
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
    if (page_topics_model_handler_) {
      page_topics_model_handler_->UnloadModel();
    }
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
  if (job->type() != AnnotationType::kPageTopics &&
      page_topics_model_handler_) {
    page_topics_model_handler_->UnloadModel();
  }
  if (job->type() != AnnotationType::kContentVisibility &&
      page_visibility_model_handler_) {
    page_visibility_model_handler_->UnloadModel();
  }

  if (job->type() == AnnotationType::kPageTopics) {
    if (!page_topics_model_handler_) {
      job->FillWithNullOutputs();
      job->OnComplete();
      job.reset();
      std::move(on_job_complete_callback).Run();
      return;
    }
    page_topics_model_handler_->ExecuteJob(std::move(on_job_complete_callback),
                                           std::move(job));
    return;
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

  if (job->type() == AnnotationType::kPageEntities) {
    if (!page_entities_model_handler_) {
      job->FillWithNullOutputs();
      job->OnComplete();
      job.reset();
      std::move(on_job_complete_callback).Run();
      return;
    }
    page_entities_model_handler_->ExecuteJob(
        std::move(on_job_complete_callback), std::move(job));
    return;
  }
  NOTREACHED();
}

}  // namespace optimization_guide
