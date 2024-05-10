// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_

#include <atomic>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/passage_embeddings_service_controller.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace page_content_annotations {
class BatchAnnotationResult;
class PageContentAnnotationsService;
}  // namespace page_content_annotations

namespace history_embeddings {

class Embedder;

// A single item that forms part of a search result; combines metadata found in
// the history embeddings database with additional info from history database.
struct ScoredUrlRow {
  explicit ScoredUrlRow(ScoredUrl scored_url)
      : scored_url(std::move(scored_url)) {}

  ScoredUrl scored_url;
  history::URLRow row;
};
using SearchResult = std::vector<ScoredUrlRow>;
using SearchResultCallback = base::OnceCallback<void(SearchResult)>;

using QualityLogEntry =
    std::unique_ptr<optimization_guide::ModelQualityLogEntry>;

class HistoryEmbeddingsService : public KeyedService,
                                 public history::HistoryServiceObserver {
 public:
  // `history_service` is never nullptr and must outlive `this`.
  // Storage uses its `history_dir() location for the database.
  HistoryEmbeddingsService(
      history::HistoryService* history_service,
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service,
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      PassageEmbeddingsServiceController* service_controller);
  HistoryEmbeddingsService(const HistoryEmbeddingsService&) = delete;
  HistoryEmbeddingsService& operator=(const HistoryEmbeddingsService&) = delete;
  ~HistoryEmbeddingsService() override;

  // Initiate async passage extraction from given host's main frame.
  // When extraction completes, the passages will be stored in the database
  // and then given to the callback.
  // Note: A `WeakDocumentPtr` is essentially a `WeakPtr<RenderFrameHost>`.
  void RetrievePassages(const history::VisitRow& visit_row,
                        content::WeakDocumentPtr weak_render_frame_host);

  // Find top `count` URL visit info entries nearest given `query`. Pass
  // results to given `callback` when search completes. Search will be narrowed
  // to a time range if `time_range_start` is provided. In that case, the
  // start of the time range is inclusive and the end is unbounded.
  // Practically, this can be thought of as [start, now) but now isn't fixed.
  void Search(std::string query,
              std::optional<base::Time> time_range_start,
              size_t count,
              SearchResultCallback callback);

  // Weak `this` provider method.
  base::WeakPtr<HistoryEmbeddingsService> AsWeakPtr();

  // Submit quality logging data after user selects an item from search result.
  void SendQualityLog(const std::string& query,
                      const SearchResult& result,
                      size_t selection,
                      size_t num_days,
                      size_t num_entered_characters,
                      bool from_omnibox_history_scope);

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class HistoryEmbeddingsBrowserTest;
  friend class HistoryEmbeddingsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(HistoryEmbeddingsServiceTest, OnHistoryDeletions);

  // A utility container to wrap anything that should be accessed on
  // the separate storage worker sequence.
  struct Storage {
    explicit Storage(const base::FilePath& storage_dir);

    // Associate the given metadata with this Storage instance. The storage is
    // not considered initialized until this metadata is supplied.
    void SetEmbedderMetadata(EmbedderMetadata metadata);

    // Called on the worker sequence to persist passages and embeddings.
    void ProcessAndStorePassages(UrlPassages url_passages,
                                 std::vector<Embedding> passages_embeddings);

    // Runs search on worker sequence.
    std::vector<ScoredUrl> Search(
        base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
        size_t query_id,
        Embedding query_embedding,
        std::optional<base::Time> time_range_start,
        size_t count);

    // Handles the History deletions on the worker thread.
    void HandleHistoryDeletions(bool for_all_history,
                                history::URLRows deleted_rows,
                                std::set<history::VisitID> deleted_visit_ids);

    // A VectorDatabase implementation that holds data in memory.
    VectorDatabaseInMemory vector_database;

    // The underlying SQL database for persistent storage.
    SqlDatabase sql_database;
  };

  // Called when the embedder metadata is available. Passes the metadata to
  // the internal storage.
  void OnEmbedderMetadataReady(EmbedderMetadata metadata);

  // This can be overridden to prepare a log entry that will then be filled
  // with data and sent on destruction. Default implementation returns null.
  virtual QualityLogEntry PrepareQualityLogEntry();

  // Called indirectly via RetrievePassages when passage extraction completes.
  void OnPassagesRetrieved(UrlPassages url_passages,
                           std::vector<std::string> passages);

  // Invoked after the embeddings for `passages` has been computed.
  void OnPassagesEmbeddingsComputed(UrlPassages url_passages,
                                    std::vector<std::string> passages,
                                    std::vector<Embedding> passages_embeddings);

  // Invoked after the embedding for the original search query has been
  // computed.
  void OnQueryEmbeddingComputed(std::optional<base::Time> time_range_start,
                                size_t count,
                                SearchResultCallback callback,
                                std::vector<std::string> query_passages,
                                std::vector<Embedding> query_embedding);

  // Finishes a search result by combining found data with additional data from
  // history database. Moves each ScoredUrl into a more complete structure with
  // a history URLRow. Omits any entries that don't have corresponding data in
  // the history database.
  void OnSearchCompleted(SearchResultCallback callback,
                         std::vector<ScoredUrl> scored_urls);

  // Calls `page_content_annotation_service_` to determine whether the passage
  // of each ScoredUrl should be shown to the user.
  void DeterminePassageVisibility(SearchResultCallback callback,
                                  std::vector<ScoredUrl> scored_urls);

  // Called after `page_content_annotation_service_` has determined visibility
  // for the passage of each ScoredUrl. This will filter `scored_urls` to only
  // contain entries that can be shown to the user.
  void OnPassageVisibilityCalculated(
      SearchResultCallback callback,
      std::vector<ScoredUrl> scored_urls,
      const std::vector<page_content_annotations::BatchAnnotationResult>&
          annotation_results);

  // The history service is used to fill in details about URLs and visits
  // found via search. It strictly outlives this due to the dependency
  // specified in HistoryEmbeddingsServiceFactory.
  raw_ptr<history::HistoryService> history_service_;

  // The page content annotations service is used to determine whether the
  // content is safe. It strictly outlives this due to the dependency specified
  // in `HistoryEmbeddingsServiceFactory`. Can be nullptr if the underlying
  // capabilities are not supported.
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;

  // Tracks the observed history service, for cleanup.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // The embedder used to compute embeddings.
  std::unique_ptr<Embedder> embedder_;

  // Metadata about the embedder.
  std::optional<EmbedderMetadata> embedder_metadata_;

  // Storage is bound to a separate sequence.
  // This will be null if the feature flag is disabled.
  base::SequenceBound<Storage> storage_;

  // Callback called when `ProcessAndStorePassages` completes. Needed for tests
  // as the blink dependency doesn't have a 'wait for pending requests to
  // complete' mechanism.
  base::RepeatingCallback<void(UrlPassages)> callback_for_tests_ =
      base::DoNothing();

  // A thread-safe invalidation mechanism to halt searches for stale queries:
  // Each query is run with the current `query_id_` and a weak pointer to the
  // atomic value itself. When it changes, any queries other than the latest
  // can be halted. Note this is not task cancellation, it breaks the inner
  // search loop while running so the atomic is needed for thread safety.
  std::atomic<size_t> query_id_;
  base::WeakPtrFactory<std::atomic<size_t>> query_id_weak_ptr_factory_;

  base::WeakPtrFactory<HistoryEmbeddingsService> weak_ptr_factory_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
