// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/continuous_search/browser/search_result_extractor_client.h"
#include "components/continuous_search/browser/search_result_extractor_client_status.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/content/browser/page_content_annotator.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace optimization_guide {

class LocalPageEntitiesMetadataProvider;
class OptimizationGuideModelProvider;
class PageContentAnnotationsModelManager;
class PageContentAnnotationsServiceTest;
class PageContentAnnotationsServiceBrowserTest;
class PageContentAnnotationsWebContentsObserver;

// The information used by HistoryService to identify a visit to a URL.
struct HistoryVisit {
  HistoryVisit();
  HistoryVisit(base::Time nav_entry_timestamp, GURL url, int64_t navigation_id);
  ~HistoryVisit();
  HistoryVisit(const HistoryVisit&);

  base::Time nav_entry_timestamp;
  GURL url;
  int64_t navigation_id = 0;
  absl::optional<std::string> text_to_annotate;

  struct Comp {
    bool operator()(const HistoryVisit& lhs, const HistoryVisit& rhs) const {
      if (lhs.nav_entry_timestamp != rhs.nav_entry_timestamp)
        return lhs.nav_entry_timestamp < rhs.nav_entry_timestamp;
      return lhs.url < rhs.url;
    }
  };
};

// The information about a search visit to store in HistoryService.
struct SearchMetadata {
  GURL normalized_url;
  std::u16string search_terms;
};

// A KeyedService that annotates page content.
class PageContentAnnotationsService : public KeyedService,
                                      public EntityMetadataProvider {
 public:
  PageContentAnnotationsService(
      const std::string& application_locale,
      OptimizationGuideModelProvider* optimization_guide_model_provider,
      history::HistoryService* history_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~PageContentAnnotationsService() override;
  PageContentAnnotationsService(const PageContentAnnotationsService&) = delete;
  PageContentAnnotationsService& operator=(
      const PageContentAnnotationsService&) = delete;

  // This is the main entry point for page content annotations by external
  // callers. Callers must call |RequestAndNotifyWhenModelAvailable| as close to
  // session start as possible to allow time for the model file to be
  // downloaded.
  void BatchAnnotate(BatchAnnotationCallback callback,
                     const std::vector<std::string>& inputs,
                     AnnotationType annotation_type);

  // Calls |BatchAnnotate| with pre-processing the hosts into tokens, all
  // specific to PageTopics.
  void BatchAnnotatePageTopics(BatchAnnotationCallback callback,
                               const std::vector<std::string>& inputs);

  // Requests that the given model for |type| be loaded in the background and
  // then runs |callback| with true when the model is ready to execute. If the
  // model is ready now, the callback is run immediately. If the model file will
  // never be available, the callback is run with false.
  void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback);

  // Returns the model info for the given annotation type, if the model file is
  // available.
  absl::optional<ModelInfo> GetModelInfoForType(AnnotationType type) const;

  // EntityMetadataProvider:
  void GetMetadataForEntityId(
      const std::string& entity_id,
      EntityMetadataRetrievedCallback callback) override;

  // Overrides the PageContentAnnotator for testing. See
  // test_page_content_annotator.h for an implementation designed for testing.
  void OverridePageContentAnnotatorForTesting(PageContentAnnotator* annotator);

 private:
  friend class PageContentAnnotationsServiceTest;
  static std::string StringInputForPageTopicsHost(const std::string& host);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Callback invoked when |visit| has been annotated.
  void OnPageContentAnnotated(
      const HistoryVisit& visit,
      const absl::optional<history::VisitContentModelAnnotations>&
          content_annotations);

  // Runs the page annotation models available to |model_manager_| on all the
  // visits within |current_visit_annotation_batch_|.
  void AnnotateVisitBatch();

  // Callback run after the annotations for a |visit| of a batch has been
  // determined. |current_visit_annotation_batch_| is updated to remove
  // the annotated visit and will trigger the next visit to be annotated.
  void OnBatchVisitAnnotated(
      const HistoryVisit& visit,
      const absl::optional<history::VisitContentModelAnnotations>&
          content_annotations);

  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;

#endif

  // The annotator to use for requests to |BatchAnnotate|. In prod, this is
  // simply |model_manager_.get()| but is set as a separate pointer here in
  // order to be override-able for testing.
  raw_ptr<PageContentAnnotator> annotator_;

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
  virtual void Annotate(const HistoryVisit& visit);

  // Creates a HistoryVisit based on the current state of |web_contents|.
  static HistoryVisit CreateHistoryVisitFromWebContents(
      content::WebContents* web_contents,
      int64_t navigation_id);

  // Persist |search_metadata| for |visit| in |history_service_|.
  virtual void PersistSearchMetadata(const HistoryVisit& visit,
                                     const SearchMetadata& search_metadata);

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

  // Persist |entities| for |visit| in |history_service_|.
  //
  // Virtualized for testing.
  virtual void PersistRemotePageEntities(
      const HistoryVisit& visit,
      const std::vector<history::VisitContentModelAnnotations::Category>&
          entities);

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

  // Runs a batch annotation validation, that is calls |BatchAnnotate| with
  // dummy input and discards the output.
  void RunBatchAnnotationValidation();

  // A metadata-only provider for page entities (as opposed to |model_manager_|
  // which does both entity model execution and metadata providing) that uses a
  // local database to provide the metadata for a given entity id. This is only
  // non-null and initialized when its feature flag is enabled.
  std::unique_ptr<LocalPageEntitiesMetadataProvider>
      local_page_entities_metadata_provider_;

  // The history service to write content annotations to. Not owned. Guaranteed
  // to outlive |this|.
  raw_ptr<history::HistoryService> history_service_;
  // The task tracker to keep track of tasks to query |history_service|.
  base::CancelableTaskTracker history_service_task_tracker_;
  // The client of continuous_search::mojom::SearchResultExtractor interface
  // used for extracting data from the main frame of Google SRP |web_contents|.
  continuous_search::SearchResultExtractorClient
      search_result_extractor_client_;
  // A LRU Cache keeping track of the visits that have been requested for
  // annotation. If the requested visit is in this cache, the models will not be
  // requested for another annotation on the same visit.
  base::LRUCache<HistoryVisit, bool, HistoryVisit::Comp>
      last_annotated_history_visits_;

  // A LRU cache of the annotation results for visits. If the text of the visit
  // is in the cache, the cached model annotations will be used.
  base::HashingLRUCache<std::string, history::VisitContentModelAnnotations>
      annotated_text_cache_;

  // The set of visits to be annotated, this is added to by Annotate requests
  // from the web content observer. These will be annotated when the set is full
  // and annotations can be scheduled with minimal impact to browsing.
  std::vector<HistoryVisit> visits_to_annotate_;

  // The batch of visits being annotated. If this is empty, it is assumed that
  // no visits are actively be annotated and a new batch can be started.
  std::vector<HistoryVisit> current_visit_annotation_batch_;

  // Is only ever set when the feature is enabled.
  std::unique_ptr<base::OneShotTimer> validation_timer_;

  base::WeakPtrFactory<PageContentAnnotationsService> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
