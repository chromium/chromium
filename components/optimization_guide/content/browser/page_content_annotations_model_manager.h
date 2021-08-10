// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_

#include "components/history/core/browser/url_row.h"
#include "components/optimization_guide/core/bert_model_executor.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

class OptimizationGuideModelProvider;
class PageEntitiesModelExecutor;

// Callback to inform the caller that the page content has been annotated.
using PageContentAnnotatedCallback = base::OnceCallback<void(
    const absl::optional<history::VisitContentModelAnnotations>&)>;

// Manages the loading and execution of models used to annotate page content.
class PageContentAnnotationsModelManager {
 public:
  explicit PageContentAnnotationsModelManager(
      OptimizationGuideModelProvider* optimization_guide_model_provider);
  ~PageContentAnnotationsModelManager();
  PageContentAnnotationsModelManager(
      const PageContentAnnotationsModelManager&) = delete;
  PageContentAnnotationsModelManager& operator=(
      const PageContentAnnotationsModelManager&) = delete;

  // Requests to annotate |text|, will invoke |callback| when completed.
  //
  // This will execute all supported models based on the models_to_execute
  // param on the PageContentAnnotationsService feature.
  void Annotate(const std::string& text, PageContentAnnotatedCallback callback);

  // Returns the version of the page topics model that is currently being used
  // to annotate page content. Will return |absl::nullopt| if no model is being
  // used to annotate page topics for received page content.
  absl::optional<int64_t> GetPageTopicsModelVersion() const;

 private:
  friend class PageContentAnnotationsModelManagerTest;

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
      PageContentAnnotatedCallback callback,
      size_t current_model_index,
      const absl::optional<std::vector<tflite::task::core::Category>>& output);

  // Set up the machinery for execution of the page topics model. This should
  // only be run at construction.
  void SetUpPageTopicsModel(
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

  // The model executor responsible for executing the page topics model.
  //
  // Can be nullptr if the page topics model will not be running for the
  // session.
  std::unique_ptr<BertModelExecutorHandle> page_topics_model_executor_handle_;

  // The model executor responsible for executing the page entities model.
  //
  // Can be nullptr if the page entities model will not be running for the
  // session.
  std::unique_ptr<PageEntitiesModelExecutor> page_entities_model_executor_;

  // The ordering of models to execute on the page content of each page load.
  std::vector<proto::OptimizationTarget> ordered_models_to_execute_;

  base::WeakPtrFactory<PageContentAnnotationsModelManager> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
