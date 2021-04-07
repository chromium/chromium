// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace optimization_guide {

class OptimizationGuideDecider;
class PageContentAnnotationsModelManager;

// The information used by HistoryService to identify a visit to a URL.
struct HistoryVisit {
  base::Time nav_entry_timestamp;
  GURL url;
};

// A KeyedService that annotates page content.
class PageContentAnnotationsService : public KeyedService {
 public:
  explicit PageContentAnnotationsService(
      OptimizationGuideDecider* optimization_guide_decider,
      history::HistoryService* history_service);
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
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Callback invoked when |visit| has been annotated.
  void OnPageContentAnnotated(
      const HistoryVisit& visit,
      const base::Optional<history::VisitContentModelAnnotations>&
          content_annotations);

  // Callback invoked when |history_service| has returned results for the visits
  // to a URL.
  void OnURLQueried(
      const HistoryVisit& visit,
      const history::VisitContentModelAnnotations& content_annotations,
      history::QueryURLResult url_result);

  // The history service to write content annotations to. Not owned. Guaranteed
  // to outlive |this|.
  history::HistoryService* history_service_;
  // The task tracker to keep track of tasks to query |history_service|.
  base::CancelableTaskTracker history_service_task_tracker_;

  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;
#endif

  base::WeakPtrFactory<PageContentAnnotationsService> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
