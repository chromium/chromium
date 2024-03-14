// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/page_content_annotations/core/page_content_annotator.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/page_content_annotations/core/page_content_annotation_job.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_visibility_model_handler.h"
#include "net/base/priority_queue.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace page_content_annotations {

// Manages the loading and execution of models used to annotate page content.
class PageContentAnnotationsModelManager : public PageContentAnnotator {
 public:
  explicit PageContentAnnotationsModelManager(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider);
  ~PageContentAnnotationsModelManager() override;
  PageContentAnnotationsModelManager(
      const PageContentAnnotationsModelManager&) = delete;
  PageContentAnnotationsModelManager& operator=(
      const PageContentAnnotationsModelManager&) = delete;

  // Each call to |Annotate| starts one job which are are run one at a time
  // based on their original order and priority. Each job runs to completion
  // before the next one will start. Thus, it's ok to call |Annotate| multiple
  // times in a row, like in a loop, and only one job will run at a time.
  //
  // PageContentAnnotator:
  void Annotate(BatchAnnotationCallback callback,
                const std::vector<std::string>& inputs,
                AnnotationType annotation_type) override;
  std::optional<optimization_guide::ModelInfo> GetModelInfoForType(
      AnnotationType type) const override;
  void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback) override;

 private:
  friend class PageContentAnnotationsModelManagerTest;

  // The supported priorities of jobs in |job_queue_|. Higher values correspond
  // to higher priorities (that is, more urgent).
  //
  // These values are not persisted anywhere and may be changed in code at any
  // time.
  enum class JobPriority {
    kUnknown = 0,

    // All publicly posted jobs will have this priority level.
    kNormal = 1,

    // Always keep this last and as the highest priority + 1. This value is
    // passed to the priority queue ctor as "how many level of priorities are
    // supported" by the queue.
    kCount = 2,
  };

  // Enumerated state machine of job execution.
  enum class JobExecutionState {
    kUnknown = 0,
    kIdle = 1,
    kRunning = 2,
    kComplete = 3,
  };

  // Set up the machinery for execution of the page visibility model. This
  // should only be run at construction.
  void SetUpPageVisibilityModel(
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider);

  // Runs the next job in |job_queue_| if there is any.
  void MaybeStartNextAnnotationJob();

  // Called when a |job| finishes executing, just before it is deleted.
  void OnJobExecutionComplete();

  // The model handler for the page visibility model.
  std::unique_ptr<PageVisibilityModelHandler> page_visibility_model_handler_;

  // The queue of all jobs to be executed. This data structure supports FIFO
  // ordering for elements of the same priority.
  using JobQueue =
      net::PriorityQueue<std::unique_ptr<PageContentAnnotationJob>>;
  JobQueue job_queue_{static_cast<size_t>(JobPriority::kCount)};

  // The current state of the running job, if any.
  JobExecutionState job_state_ = JobExecutionState::kIdle;

  // The model provider, not owned.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider>
      optimization_guide_model_provider_;

  base::WeakPtrFactory<PageContentAnnotationsModelManager> weak_ptr_factory_{
      this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
