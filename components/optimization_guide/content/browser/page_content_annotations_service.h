// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/continuous_search/browser/search_result_extractor_client.h"
#include "components/continuous_search/browser/search_result_extractor_client_status.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations_common.h"
#include "components/optimization_guide/content/browser/page_content_annotator.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace optimization_guide {

class OptimizationGuideModelProvider;
class PageContentAnnotationsModelManager;
class PageContentAnnotationsServiceBrowserTest;
class PageContentAnnotationsWebContentsObserver;

// The information used by HistoryService to identify a visit to a URL.
struct HistoryVisit {
  base::Time nav_entry_timestamp;
  GURL url;
  int64_t navigation_id;

  struct Comp {
    bool operator()(const HistoryVisit& lhs, const HistoryVisit& rhs) const {
      if (lhs.nav_entry_timestamp != rhs.nav_entry_timestamp)
        return lhs.nav_entry_timestamp < rhs.nav_entry_timestamp;
      return lhs.url < rhs.url;
    }
  };
};

// A KeyedService that annotates page content.
class PageContentAnnotationsService : public KeyedService,
                                      public EntityMetadataProvider {
 public:
  explicit PageContentAnnotationsService(
      OptimizationGuideModelProvider* optimization_guide_model_provider,
      history::HistoryService* history_service);
  ~PageContentAnnotationsService() override;
  PageContentAnnotationsService(const PageContentAnnotationsService&) = delete;
  PageContentAnnotationsService& operator=(
      const PageContentAnnotationsService&) = delete;

  // This is the main entry point for page content annotations by external
  // callers.
  //
  // TODO(crbug/1249632): Flesh out description more as implementation
  // progresses and we see what is most important to write here.
  void BatchAnnotate(BatchAnnotationCallback callback,
                     const std::vector<std::string>& inputs,
                     AnnotationType annotation_type);

  // Overrides the PageContentAnnotator for testing. See
  // test_page_content_annotator.h for an implementation designed for testing.
  void OverridePageContentAnnotatorForTesting(
      std::unique_ptr<PageContentAnnotator> annotator);

  // Returns the version of the page topics model that is currently being used
  // to annotate page content. Will return |absl::nullopt| if no model is being
  // used to annotate page topics for received page content.
  absl::optional<int64_t> GetPageTopicsModelVersion() const;

  // EntityMetadataProvider:
  void GetMetadataForEntityId(
      const std::string& entity_id,
      EntityMetadataRetrievedCallback callback) override;

 private:
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Callback invoked when |visit| has been annotated.
  void OnPageContentAnnotated(
      const HistoryVisit& visit,
      const absl::optional<history::VisitContentModelAnnotations>&
          content_annotations);

  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;
#endif

  // TODO(crbug/1249632): This will take the place of |model_manager_| where
  // |PageContentAnnotationsModelManager| implements the virtual interface.
  //
  // The annotator to use for requests to |BatchAnnotate|. Override-able for
  // testing.
  std::unique_ptr<PageContentAnnotator> annotator_;

  // Requests to annotate |text|, which is associated with |web_contents|.
  //
  // When finished annotating, it will store the relevant information in
  // History Service.
  //
  // The WCO friend class is used to keep the `Annotate` API internal to
  // OptGuide. Callers should use `BatchAnnotate` instead.
  friend class PageContentAnnotationsWebContentsObserver;
  friend class PageContentAnnotationsServiceBrowserTest;
  // Virtualized for testing.
  virtual void Annotate(const HistoryVisit& visit, const std::string& text);

  // Creates a HistoryVisit based on the current state of |web_contents|.
  static HistoryVisit CreateHistoryVisitFromWebContents(
      content::WebContents* web_contents,
      int64_t navigation_id);

  // Requests |search_result_extractor_client_| to extract related searches from
  // the Google SRP DOM associated with |web_contents|.
  //
  // Once finished, it will store the related searches in History Service.
  //
  // Virtualized for testing.
  virtual void ExtractRelatedSearches(const HistoryVisit& visit,
                                      content::WebContents* web_contents);

  // Callback invoked when related searches have been extracted for |visit|.
  void OnRelatedSearchesExtracted(
      const HistoryVisit& visit,
      continuous_search::SearchResultExtractorClientStatus status,
      continuous_search::mojom::CategoryResultsPtr results);

  using PersistAnnotationsCallback = base::OnceCallback<void(history::VisitID)>;
  // Queries |history_service| for all the visits to the visited URL of |visit|.
  // |callback| will be invoked to write the bound content annotations to
  // |history_service| once the visits to the given URL have returned.
  void QueryURL(const HistoryVisit& visit, PersistAnnotationsCallback callback);
  // Callback invoked when |history_service| has returned results for the visits
  // to a URL. In turn invokes |callback| to write the bound content annotations
  // to |history_service|.
  void OnURLQueried(const HistoryVisit& visit,
                    PersistAnnotationsCallback callback,
                    history::QueryURLResult url_result);

  // The history service to write content annotations to. Not owned. Guaranteed
  // to outlive |this|.
  history::HistoryService* history_service_;
  // The task tracker to keep track of tasks to query |history_service|.
  base::CancelableTaskTracker history_service_task_tracker_;
  // The client of continuous_search::mojom::SearchResultExtractor interface
  // used for extracting data from the main frame of Google SRP |web_contents|.
  continuous_search::SearchResultExtractorClient
      search_result_extractor_client_;
  // A MRU Cache keeping track of the visits that have been requested for
  // annotation. If the requested visit is in this cache, the models will not be
  // requested for another annotation on the same visit.
  base::MRUCache<HistoryVisit, bool, HistoryVisit::Comp>
      last_annotated_history_visits_;

  base::WeakPtrFactory<PageContentAnnotationsService> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
