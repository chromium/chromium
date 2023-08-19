// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/content/browser/page_content_annotator.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/page_content_annotation_job.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "components/optimization_guide/core/page_visibility_model_handler.h"
#include "components/optimization_guide/core/text_embedding_model_handler.h"
#include "net/base/priority_queue.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

class OptimizationGuideModelProvider;
class PageEntitiesModelHandler;

// Callback to inform the caller that the metadata for an entity ID has been
// retrieved.
using EntityMetadataRetrievedCallback =
    base::OnceCallback<void(const absl::optional<EntityMetadata>&)>;

// Manages the loading and execution of models used to annotate page content.
class PageContentAnnotationsModelManager : public PageContentAnnotator {
 public:
  explicit PageContentAnnotationsModelManager(
      OptimizationGuideModelProvider* optimization_guide_model_provider);
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
  absl::optional<ModelInfo> GetModelInfoForType(
      AnnotationType type) const override;
  void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback) override;

  // Retrieves the metadata associated with |entity_id|. Invokes |callback|
  // when done.
  void GetMetadataForEntityId(const std::string& entity_id,
                              EntityMetadataRetrievedCallback callback);

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

  // Set up the machinery for execution of the page entities model. This should
  // only be run at construction. Runs |callback(true)| when the model executor
  // has a model file or |callback(false)| if the model executor is not
  // available.
  void SetUpPageEntitiesModel(OptimizationGuideModelProvider* model_provider,
                              base::OnceCallback<void(bool)> callback);

  // Set up the machinery for execution of the page visibility model. This
  // should only be run at construction.
  void SetUpPageVisibilityModel(
      OptimizationGuideModelProvider* optimization_guide_model_provider);

  // Set up the machinery for execution of the page visibility model. This
  // should only be run at construction.
  void SetUpTextEmbeddingModel(
      OptimizationGuideModelProvider* optimization_guide_model_provider);

  // Overrides |page_entities_model_handler_| for testing purposes.
  void OverridePageEntitiesModelHandlerForTesting(
      std::unique_ptr<PageEntitiesModelHandler> page_entities_model_handler);

  // Runs the next job in |job_queue_| if there is any.
  void MaybeStartNextAnnotationJob();

  // Called when a |job| finishes executing, just before it is deleted.
  void OnJobExecutionComplete();

  // The model handler for the page visibility model.
  std::unique_ptr<PageVisibilityModelHandler> page_visibility_model_handler_;

  // The model handler responsible for executing the page entities model.
  //
  // Can be nullptr if the page entities model will not be running for the
  // session.
  std::unique_ptr<PageEntitiesModelHandler> page_entities_model_handler_;

  // The model handler responsible for executing the text embedding model.
  std::unique_ptr<TextEmbeddingModelHandler> text_embedding_model_handler_;

  // The queue of all jobs to be executed. This data structure supports FIFO
  // ordering for elements of the same priority.
  using JobQueue =
      net::PriorityQueue<std::unique_ptr<PageContentAnnotationJob>>;
  JobQueue job_queue_{static_cast<size_t>(JobPriority::kCount)};

  // The current state of the running job, if any.
  JobExecutionState job_state_ = JobExecutionState::kIdle;

  // The model provider, not owned.
  raw_ptr<OptimizationGuideModelProvider> optimization_guide_model_provider_;

  base::WeakPtrFactory<PageContentAnnotationsModelManager> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
