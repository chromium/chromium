// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_

#include "components/optimization_guide/content/browser/bert_model_executor.h"
#include "components/optimization_guide/content/browser/page_content_annotations.h"

namespace optimization_guide {

class OptimizationGuideDecider;

// Callback to inform the caller that the page content has been annotated.
using PageContentAnnotatedCallback =
    base::OnceCallback<void(const base::Optional<PageContentAnnotations>&)>;

// Manages the loading and execution of models used to annotate page content.
class PageContentAnnotationsModelManager {
 public:
  explicit PageContentAnnotationsModelManager(
      OptimizationGuideDecider* optimization_guide_decider);
  ~PageContentAnnotationsModelManager();
  PageContentAnnotationsModelManager(
      const PageContentAnnotationsModelManager&) = delete;
  PageContentAnnotationsModelManager& operator=(
      const PageContentAnnotationsModelManager&) = delete;

  // Requests to annotate |text|, will invoke |callback| when completed.
  void Annotate(const std::string& text, PageContentAnnotatedCallback callback);

  // Returns the version of the page topics model that is currently being used
  // to annotate page content. Will return |base::nullopt| if no model is being
  // used to annotate page topics for received page content.
  base::Optional<int64_t> GetPageTopicsModelVersion() const;

 private:
  // Invoked when the page topics model has finished executing.
  void OnPageTopicsModelExecutionCompleted(
      PageContentAnnotatedCallback callback,
      const base::Optional<std::vector<tflite::task::core::Category>>& output);

  // The model executor responsible for executing the page topics model.
  std::unique_ptr<BertModelExecutor> page_topics_model_executor_;

  base::WeakPtrFactory<PageContentAnnotationsModelManager> weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_MODEL_MANAGER_H_
