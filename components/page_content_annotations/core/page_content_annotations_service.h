// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "components/continuous_search/common/search_result_extractor_client_status.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/common/zero_suggest_cache_service_interface.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/optimization_guide/proto/salient_image_metadata.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotator.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

class OptimizationGuideLogger;

namespace history {
class HistoryService;
}  // namespace history

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationGuideModelProvider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace page_content_annotations {

class PageContentAnnotationsModelManager;
class PageContentAnnotationsServiceBrowserTest;
class PageContentAnnotationsValidator;
class PageContentAnnotationsWebContentsObserver;

// The information used by HistoryService to identify a visit to a URL.
struct HistoryVisit {
  HistoryVisit();
  HistoryVisit(base::Time nav_entry_timestamp, GURL url);
  explicit HistoryVisit(history::VisitID visit_id);
  ~HistoryVisit();
  HistoryVisit(const HistoryVisit&);

  base::Time nav_entry_timestamp;
  GURL url;
  int64_t navigation_id = 0;
  std::optional<history::VisitID> visit_id;
  std::optional<std::string> text_to_annotate;

  struct Comp {
    bool operator()(const HistoryVisit& lhs, const HistoryVisit& rhs) const {
      // The synced visits in history can be merged in any order to local model
      // store. So, visit ID may not match the timeline order of visits. Most
      // common case is to fetch the latest visit first and are assigned lower
      // visit IDs. Prioritize timestamp for the comparison.
      if (lhs.nav_entry_timestamp != rhs.nav_entry_timestamp) {
        return lhs.nav_entry_timestamp < rhs.nav_entry_timestamp;
      }
      if (lhs.visit_id && rhs.visit_id) {
        return *lhs.visit_id < *rhs.visit_id;
      }
      if (lhs.visit_id) {
        // If we get here, this means that |rhs| does not have a visit ID.
        return false;
      }
      return lhs.url < rhs.url;
    }
  };
};

// The type of page content annotations stored in the history database.
enum class PageContentAnnotationsType {
  kUnknown = 0,
  // Results from executing the models on page content or annotations received
  // from the remote Optimization Guide service.
  kModelAnnotations = 1,
  // Related searches for the Google Search Results page.
  kRelatedSearches = 2,
  // Metadata for "search-like" pages.
  kSearchMetadata = 3,
  // Metadata received from the remote Optimization Guide service.
  kRemoteMetdata = 4,
  // Salient image metadata.
  kSalientImageMetadata = 5,

  // New entries should be added to the PageContentAnnotationsStorageType in
  // optimization/histograms.xml.
};

// A KeyedService that annotates page content.
class PageContentAnnotationsService
    : public KeyedService,
      public history::HistoryServiceObserver,
      public ZeroSuggestCacheServiceInterface::Observer {
 public:
  // Observer interface to listen for PageContentAnnotations for page loads.
  // Annotations will be sent for each page load for the registered annotation
  // type.
  class PageContentAnnotationsObserver : public base::CheckedObserver {
   public:
    virtual void OnPageContentAnnotated(
        const GURL& url,
        const PageContentAnnotationsResult& result) = 0;
  };

  PageContentAnnotationsService(
      const std::string& application_locale,
      const std::string& country_code,
      optimization_guide::OptimizationGuideModelProvider*
          optimization_guide_model_provider,
      history::HistoryService* history_service,
      TemplateURLService* template_url_service,
      ZeroSuggestCacheServiceInterface* zero_suggest_cache_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const base::FilePath& database_dir,
      OptimizationGuideLogger* optimization_guide_logger,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
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

  // Requests that the given model for |type| be loaded in the background and
  // then runs |callback| with true when the model is ready to execute. If the
  // model is ready now, the callback is run immediately. If the model file will
  // never be available, the callback is run with false.
  void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback);

  // Returns the model info for the given annotation type, if the model file is
  // available.
  std::optional<optimization_guide::ModelInfo> GetModelInfoForType(
      AnnotationType type) const;

  // history::HistoryServiceObserver:
  void OnURLsModified(history::HistoryService* history_service,
                      const history::URLRows& changed_urls) override;
  void OnURLVisitedWithNavigationId(
      history::HistoryService* history_service,
      const history::URLRow& url_row,
      const history::VisitRow& visit_row,
      std::optional<int64_t> local_navigation_id) override;

  // Overrides the PageContentAnnotator for testing. See
  // test_page_content_annotator.h for an implementation designed for testing.
  void OverridePageContentAnnotatorForTesting(PageContentAnnotator* annotator);

  // Specifies whether PageContentAnnotationsService should extract "related
  // searches" data from the ZPS response cache.
  bool ShouldExtractRelatedSearchesFromZPSCache();

  // ZeroSuggestCacheServiceInterface::Observer:
  void OnZeroSuggestResponseUpdated(
      const std::string& page_url,
      const ZeroSuggestCacheServiceInterface::CacheEntry& response) override;

  // Callback used to extract "related searches" data from cached ZPS responses.
  void ExtractRelatedSearchesFromZeroSuggestResponse(
      const ZeroSuggestCacheServiceInterface::CacheEntry& response,
      history::QueryURLResult url_result);

  // Invoked when related searches have been extracted for |visit|, to store
  // the related searches in History Service.
  void OnRelatedSearchesExtracted(
      const HistoryVisit& visit,
      continuous_search::SearchResultExtractorClientStatus status,
      continuous_search::mojom::CategoryResultsPtr results);

  // Adds or removes PageContentAnnotations observers for |annotation_type|.
  void AddObserver(AnnotationType annotation_type,
                   PageContentAnnotationsObserver* observer);
  void RemoveObserver(AnnotationType annotation_type,
                      PageContentAnnotationsObserver* observer);

  OptimizationGuideLogger* optimization_guide_logger() const {
    return optimization_guide_logger_;
  }

 private:
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Callback invoked when a single |visit| has been annotated.
  void OnPageContentAnnotated(
      const HistoryVisit& visit,
      const std::optional<history::VisitContentModelAnnotations>&
          content_annotations);

  // Maybe calls |AnnotateVisitBatch| to start a new batch of content
  // annotations. Returns true if a new batch is started. Returns false if a
  // batch is already running, or if there batch queue is not full.
  bool MaybeStartAnnotateVisitBatch();

  // Runs the page annotation models available to |model_manager_| on all the
  // visits within |visits_to_annotate_|.
  void AnnotateVisitBatch();

  // Runs when a single annotation job of |type| is completed and |batch_result|
  // can be merged into |merge_to_output|. |signal_merge_complete_callback|
  // should be run last as it is a |base::BarrierClosure| that may trigger
  // |OnBatchVisitsAnnotated| to run.
  void OnAnnotationBatchComplete(
      AnnotationType type,
      std::vector<std::optional<history::VisitContentModelAnnotations>>*
          merge_to_output,
      base::OnceClosure signal_merge_complete_callback,
      const std::vector<BatchAnnotationResult>& batch_result);

  // Callback run after all annotation types in |annotation_types_to_execute_|
  // for all of |current_visit_annotation_batch_| has been completed.
  void OnBatchVisitsAnnotated(
      std::unique_ptr<
          std::vector<std::optional<history::VisitContentModelAnnotations>>>
          merged_annotation_outputs);

  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;

#endif

  // The annotator to use for requests to |BatchAnnotate| and |Annotate|. In
  // prod, this is simply |model_manager_.get()| but is set as a separate
  // pointer here in order to be override-able for testing.
  raw_ptr<PageContentAnnotator> annotator_ = nullptr;

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

  // Annotates the provided `visit` in the history DB with the given list of
  // `related_searches`.
  //
  // Virtualized for testing.
  virtual void AddRelatedSearchesForVisit(
      const HistoryVisit& visit,
      const std::vector<std::string>& related_searches);

  // Persist |page_entities_metadata| for |visit| in |history_service_|.
  //
  // Virtualized for testing.
  virtual void PersistRemotePageMetadata(
      const HistoryVisit& visit,
      const optimization_guide::proto::PageEntitiesMetadata&
          page_entities_metadata);

  // Persist |salient_image_metadata| for |visit| in |history_service_|.
  //
  // Virtualized for testing.
  virtual void PersistSalientImageMetadata(
      const HistoryVisit& visit,
      const optimization_guide::proto::SalientImageMetadata&
          salient_image_metadata);

  using PersistAnnotationsCallback = base::OnceCallback<void(history::VisitID)>;
  // Queries |history_service| for all the visits to the visited URL of |visit|.
  // |callback| will be invoked to write the bound content annotations to
  // |history_service| once the visits to the given URL have returned. The
  // |annotation_type| of data to be stored in History Service is passed along
  // for metrics purposes.
  void QueryURL(const HistoryVisit& visit,
                PersistAnnotationsCallback callback,
                PageContentAnnotationsType annotation_type);
  // Callback invoked when |history_service| has returned results for the visits
  // to a URL. In turn invokes |callback| to write the bound content annotations
  // to |history_service|.
  void OnURLQueried(const HistoryVisit& visit,
                    PersistAnnotationsCallback callback,
                    PageContentAnnotationsType annotation_type,
                    history::QueryURLResult url_result);

  // Notifies the PageContentAnnotationsResult to the observers for
  // |annotation_type|.
  void NotifyPageContentAnnotatedObservers(
      AnnotationType annotation_type,
      const GURL& url,
      const PageContentAnnotationsResult& page_content_annotations_result);

  // Callback invoked when a response for |optimization_type| has been received
  // from |optimization_guide_decider_| for |visit|.
  void OnOptimizationGuideResponseReceived(
      const HistoryVisit& visit,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Sends the page for annotation from |OnURLVisitedWithNavigationId| and
  // |OnURLsModified|.
  void OnWaitForTitleDone(const GURL& url);

  // The minimum score that an allowlisted page category must have for it to be
  // persisted.
  const int min_page_category_score_to_persist_;

  // The history service to write content annotations to. Not owned. Guaranteed
  // to outlive |this|.
  const raw_ptr<history::HistoryService> history_service_;
  // Not owned and must outlive |this|. Can be nullptr in tests only.
  const raw_ptr<TemplateURLService> template_url_service_;
  // The scoped observation to the HistoryService.
  base::ScopedObservation<history::HistoryService,
                          PageContentAnnotationsService>
      history_service_observation_{this};
  // The task tracker to keep track of tasks to query |history_service|.
  base::CancelableTaskTracker history_service_task_tracker_;
  // The zero suggest cache service used to fetch cached ZPS response data.
  const raw_ptr<ZeroSuggestCacheServiceInterface> zero_suggest_cache_service_;
  // The scoped observation to the ZeroSuggestCacheService.
  base::ScopedObservation<ZeroSuggestCacheServiceInterface,
                          PageContentAnnotationsService>
      zero_suggest_cache_service_observation_{this};
  // A LRU cache mapping each SRP URL to the associated set of "related
  // searches" which were obtained via ZPS prefetch logic. This cache is used to
  // coordinate the SRP DOM extraction and ZPS prefetch flows to ensure that the
  // appropriate history visit is targeted for annotation.
  base::HashingLRUCache<std::string, std::vector<std::string>>
      prefetched_related_searches_;
  // A LRU Cache keeping track of the visits that have been requested for
  // annotation. If the requested visit is in this cache, the models will not be
  // requested for another annotation on the same visit.
  base::LRUCache<HistoryVisit, bool, HistoryVisit::Comp>
      last_annotated_history_visits_;

  // A LRU cache containing a set of unique |HistoryVisit|'s for any url.
  // In OnURLVisited, the HistoryVisit will be added to the map with its
  // corresponding url and we'll either wait (5 seconds) for the title to be
  // populated in OnURLsModified or call annotate with the title we already
  // have.
  base::LRUCache<GURL, std::vector<HistoryVisit>> missing_title_visits_by_url_;

  // A LRU cache of the annotation results for visits. If the text of the visit
  // is in the cache, the cached model annotations will be used.
  base::HashingLRUCache<std::string, history::VisitContentModelAnnotations>
      annotated_text_cache_;

  // The set of visits to be annotated, this is added to by Annotate requests
  // from the web content observer. These will be annotated when the set is full
  // and annotations can be scheduled with minimal impact to browsing.
  std::multiset<HistoryVisit, HistoryVisit::Comp> visits_to_annotate_;

  // Callback to run batch annotations after a timeout if the batch size is not
  // reached.
  base::CancelableOnceClosure batch_annotations_start_timer_;

  // The set of |AnnotationType|'s to run on each of |visits_to_annotate_|.
  std::vector<AnnotationType> annotation_types_to_execute_;

  // The batch of visits being annotated. If this is empty, it is assumed that
  // no visits are actively be annotated and a new batch can be started.
  std::vector<HistoryVisit> current_visit_annotation_batch_;

  // Set during this' ctor if the corresponding command line or feature flags
  // are set.
  std::unique_ptr<PageContentAnnotationsValidator> validator_;

  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_ = nullptr;

  // Whether fetching for remote page metadata enabled.
  bool is_remote_page_metadata_fetching_enabled_ = false;

  // Whether fetching for salient image metadata is enabled.
  bool is_salient_image_metadata_fetching_enabled_ = false;

  // Not owned and must outlive |this|.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  // Observers of PageContentAnnotations that have been registered per
  // AnnotationType.
  std::map<AnnotationType, base::ObserverList<PageContentAnnotationsObserver>>
      page_content_annotations_observers_;

  base::WeakPtrFactory<PageContentAnnotationsService> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_SERVICE_H_
