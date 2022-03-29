// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "components/history/core/browser/url_row.h"
#include "components/optimization_guide/content/browser/page_content_annotator.h"
#include "components/optimization_guide/core/bert_model_handler.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/page_content_annotation_job.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "components/optimization_guide/core/page_topics_model_executor.h"
#include "components/optimization_guide/core/page_visibility_model_executor.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "net/base/priority_queue.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

class OptimizationGuideModelProvider;
class PageEntitiesModelExecutor;

// Callback to inform the caller that the page content has been annotated.
using PageContentAnnotatedCallback = base::OnceCallback<void(
    const absl::optional<history::VisitContentModelAnnotations>&)>;

// Callback to inform the caller that the metadata for an entity ID has been
// retrieved.
using EntityMetadataRetrievedCallback =
    base::OnceCallback<void(const absl::optional<EntityMetadata>&)>;

// Manages the loading and execution of models used to annotate page content.
class PageContentAnnotationsModelManager : public PageContentAnnotator {
 public:
  PageContentAnnotationsModelManager(
      const std::string& application_locale,
      OptimizationGuideModelProvider* optimization_guide_model_provider);
  ~PageContentAnnotationsModelManager() override;
  PageContentAnnotationsModelManager(
      const PageContentAnnotationsModelManager&) = delete;
  PageContentAnnotationsModelManager& operator=(
      const PageContentAnnotationsModelManager&) = delete;

  // Requests to annotate |text|, will invoke |callback| when completed.
  //
  // This will execute all supported models of the PageContentAnnotationsService
  // feature and is only used by the History service code path. See the below
  // |Annotate| for the publicly available Annotation code path.
  // TODO(crbug/1278833): Remove this.
  void Annotate(const std::string& text, PageContentAnnotatedCallback callback);

  // PageContentAnnotator:
  void Annotate(BatchAnnotationCallback callback,
                const std::vector<std::string>& inputs,
                AnnotationType annotation_type) override;
  absl::optional<ModelInfo> GetModelInfoForType(
      AnnotationType type) const override;
  void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback) override;

  // Returns the version of the page topics model that is currently being used
  // to annotate page content. Will return |absl::nullopt| if no model is being
  // used to annotate page topics for received page content.
  absl::optional<int64_t> GetPageTopicsModelVersion() const;

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

    // TODO(crbug/1278833): Add a kHigh value for internal jobs.

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

  // Runs the next model on |text| based on |current_model_index|. If the
  // last model in |ordered_models_to_execute_| has executed, it will invoke
  // |callback| with the contents of |current_annotations|.
  //
  // |current_annotations|, |callback|, and |current_model_index| will be passed
  // through a series of callbacks and will be invoked after the last model
  // executes.
  void ExecuteNextModelOrInvokeCallback(
      const std::string& text,
      std::unique_ptr<history::VisitContentModelAnnotations>
          current_annotations,
      PageContentAnnotatedCallback callback,
      absl::optional<size_t> current_model_index);

  // Set up the machinery for execution of the page entities model. This should
  // only be run at construction.
  void SetUpPageEntitiesModel(OptimizationGuideModelProvider* model_provider);

  // Requests to execute the page entities model with |text|, populate
  // |current_annotations| with detected entities on success, and proceed to
  // execute any subsequent models.
  //
  // |current_annotations|, |callback|, and |current_model_index| will be passed
  // through a series of callbacks and will be invoked after the last model
  // executes.
  void ExecutePageEntitiesModel(
      const std::string& text,
      std::unique_ptr<history::VisitContentModelAnnotations>
          current_annotations,
      PageContentAnnotatedCallback callback,
      size_t current_model_index);

  // Invoked when the page entities model has finished executing. If |output| is
  // populated, |current_annotations| will be populated with the detected
  // entities. Regardless of success, this will proceed to execute any
  // subsequent models. If it is the last model to execute, it will invoke
  // |callback| with the contents of |current_annotations| after it has
  // populated it based on |output|.
  void OnPageEntitiesModelExecutionCompleted(
      const std::string& text,
      std::unique_ptr<history::VisitContentModelAnnotations>
          current_annotations,
      base::TimeTicks execution_start,
      PageContentAnnotatedCallback callback,
      size_t current_model_index,
      const absl::optional<std::vector<ScoredEntityMetadata>>& output);

  // Set up the machinery for execution of the page topics model. This should
  // only be run at construction.
  void SetUpPageTopicsModel(
      OptimizationGuideModelProvider* optimization_guide_model_provider);

  // Set up the machinery for execution of the page topics v2 model. This should
  // only be run at construction.
  // TODO(crbug/1266504): Actually call this based on a separate feature flag.
  void SetUpPageTopicsV2Model(
      OptimizationGuideModelProvider* optimization_guide_model_provider);

  // Set up the machinery for execution of the page visibility model. This
  // should only be run at construction.
  // TODO(crbug/1266504): Actually call this based on a separate feature flag.
  void SetUpPageVisibilityModel(
      OptimizationGuideModelProvider* optimization_guide_model_provider);

  // Requests to execute the page topics model with |text|, populate
  // |current_annotations| with detected topics on success, and proceed to
  // execute any subsequent models.
  //
  // |current_annotations|, |callback|, and |current_model_index| will be passed
  // through a series of callbacks and will be invoked after the last model
  // executes.
  void ExecutePageTopicsModel(
      const std::string& text,
      std::unique_ptr<history::VisitContentModelAnnotations>
          current_annotations,
      PageContentAnnotatedCallback callback,
      size_t current_model_index);

  // Invoked when the page topics model has finished executing. If |output| is
  // populated, |current_annotations| will be populated based on that.
  // Regardless of success, this will proceed to execute any subsequent models.
  // If it is the last model to execute, it will invoke |callback| with the
  // contents of |current_annotations| after it has populated it based on
  // |output|.
  void OnPageTopicsModelExecutionCompleted(
      const std::string& text,
      std::unique_ptr<history::VisitContentModelAnnotations>
          current_annotations,
      PageContentAnnotatedCallback callback,
      size_t current_model_index,
      const proto::PageTopicsModelMetadata& model_metadata,
      const absl::optional<std::vector<tflite::task::core::Category>>& output);

  // Populates |out_content_annotations| based on |model_output| and
  // |model_metadata|.
  void PopulateContentModelAnnotationsFromPageTopicsModelOutput(
      const proto::PageTopicsModelMetadata& model_metadata,
      const std::vector<tflite::task::core::Category>& model_output,
      history::VisitContentModelAnnotations* out_content_annotations) const;

  // Overrides |page_entities_model_executor_| for testing purposes.
  void OverridePageEntitiesModelExecutorForTesting(
      std::unique_ptr<PageEntitiesModelExecutor> page_entities_model_executor);

  // Runs the next job in |job_queue_| if there is any.
  void MaybeStartNextAnnotationJob();

  // Called when a |job| finishes executing, just before it is deleted.
  void OnJobExecutionComplete();

  // The model executor responsible for executing the page topics model.
  //
  // Can be nullptr if the page topics model will not be running for the
  // session.
  // TODO(crbug/1266504): Deprecate this in favor of
  // |on_demand_page_topics_model_executor_|.
  std::unique_ptr<BertModelHandler> page_topics_model_handler_;

  // The model executor responsible for executing the on demand page topics
  // model.
  // TODO(crbug/1266504): Replace |page_topics_model_handler_| with
  // this.
  std::unique_ptr<PageTopicsModelExecutor>
      on_demand_page_topics_model_executor_;

  // The model executor responsible for executing the page visibility model.
  std::unique_ptr<PageVisibilityModelExecutor> page_visibility_model_executor_;

  // The model executor responsible for executing the page entities model.
  //
  // Can be nullptr if the page entities model will not be running for the
  // session.
  std::unique_ptr<PageEntitiesModelExecutor> page_entities_model_executor_;

  // The ordering of models to execute on the page content of each page load.
  std::vector<proto::OptimizationTarget> ordered_models_to_execute_;

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
