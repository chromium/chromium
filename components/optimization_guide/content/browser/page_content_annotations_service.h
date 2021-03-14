// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {

class OptimizationGuideDecider;
class PageContentAnnotationsModelManager;

// The information used by HistoryService to identify a visit.
struct HistoryVisit {
  // TODO(crbug/1177102): Add history::ContextID.
  int nav_entry_id;
  const GURL& url;
};

// A KeyedService that annotates page content.
class PageContentAnnotationsService : public KeyedService {
 public:
  explicit PageContentAnnotationsService(
      OptimizationGuideDecider* optimization_guide_decider);
  ~PageContentAnnotationsService() override;
  PageContentAnnotationsService(const PageContentAnnotationsService&) = delete;
  PageContentAnnotationsService& operator=(
      const PageContentAnnotationsService&) = delete;

  // Creates a HistoryVisit based on the current state of |web_contents|.
  static HistoryVisit CreateHistoryVisitFromWebContents(
      content::WebContents* web_contents);

  // Requests to annotate |text|, which is associated with |web_contents|.
  //
  // When finished annotating, it will store the relevant information in
  // History Service.
  //
  // Virtualized for testing.
  virtual void Annotate(const HistoryVisit& visit, const std::string& text);

  // Returns the version of the page topics model that is currently being used
  // to annotate page content. Will return |base::nullopt| if no model is being
  // used to annotate page topics for received page content.
  base::Optional<int64_t> GetPageTopicsModelVersion() const;

 private:
  // Callback invoked when |visit| has been annotated.
  void OnPageContentAnnotated(
      const HistoryVisit& visit,
      const base::Optional<PageContentAnnotations>& page_content_annotations);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;
#endif

  base::WeakPtrFactory<PageContentAnnotationsService> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
