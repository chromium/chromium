// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_

#include "components/history/core/browser/url_row.h"
#include "components/optimization_guide/core/bert_model_executor.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"

namespace optimization_guide {

class OptimizationGuideModelProvider;

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
  void Annotate(const std::string& text, PageContentAnnotatedCallback callback);

  // Returns the version of the page topics model that is currently being used
  // to annotate page content. Will return |absl::nullopt| if no model is being
  // used to annotate page topics for received page content.
  absl::optional<int64_t> GetPageTopicsModelVersion() const;

 private:
  friend class PageContentAnnotationsModelManagerTest;

  // Invoked when the page topics model has finished executing.
  void OnPageTopicsModelExecutionCompleted(
      PageContentAnnotatedCallback callback,
      const proto::PageTopicsModelMetadata& model_metadata,
      const absl::optional<std::vector<tflite::task::core::Category>>& output);

  // Converts |model_output| into content model annotations based on
  // |model_metadata|.
  history::VisitContentModelAnnotations GetContentModelAnnotationsFromOutput(
      const proto::PageTopicsModelMetadata& model_metadata,
      const std::vector<tflite::task::core::Category>& model_output) const;

  // The model executor responsible for executing the page topics model.
  std::unique_ptr<BertModelExecutorHandle> page_topics_model_executor_handle_;

  base::WeakPtrFactory<PageContentAnnotationsModelManager> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
