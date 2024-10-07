// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_

#include <atomic>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback_list.h"
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
#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/intent_classifier.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"

class HistoryEmbeddingsInteractiveTest;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace page_content_annotations {
class BatchAnnotationResult;
class PageContentAnnotationsService;
}  // namespace page_content_annotations

namespace os_crypt_async {
class OSCryptAsync;
}

namespace history_embeddings {

class Embedder;

// Counts the # of ' ' vanilla-space characters in `s`.
// TODO(crbug.com/343256907): Should work on international inputs which may:
//   a) Use special whitespace, OR
//   b) Not use whitespace for word breaks (e.g. Thai).
//   `String16VectorFromString16()` is the omnibox solution. We could probably
//   just replace-all `CountWords(s)` ->
//   `String16VectorFromString16(CleanUpTitleForMatching(s, nullptr)).size()`.
size_t CountWords(const std::string& s);

// A single item that forms part of a search result; combines metadata found in
// the history embeddings database with additional info from history database.
struct ScoredUrlRow {
  explicit ScoredUrlRow(ScoredUrl scored_url);
  ScoredUrlRow(const ScoredUrlRow&);
  ScoredUrlRow(ScoredUrlRow&&);
  ~ScoredUrlRow();
  ScoredUrlRow& operator=(const ScoredUrlRow&);
  ScoredUrlRow& operator=(ScoredUrlRow&&);

  // Returns the highest scored passage in `passages_embeddings`.
  std::string GetBestPassage() const;

  // Finds the indices of the top scores, ordered descending by score.
  // This is useful for selecting a subset of `passages_embeddings` for use as
  // answerer context. The size of the returned vector will be at least
  // `min_count` provided there is sufficient data available. The
  // `min_word_count` parameter will also be used to ensure the
  // passages for returned indices have word counts adding up to at
  // least this minimum.
  std::vector<size_t> GetBestScoreIndices(size_t min_count,
                                          size_t min_word_count) const;

  // Basic scoring and history data for this URL.
  ScoredUrl scored_url;
  history::URLRow row;

  // All passages and embeddings for this URL (i.e. not a partial set).
  UrlPassagesEmbeddings passages_embeddings;

  // All scores against the query for `passages_embeddings`.
  std::vector<float> scores;
};

struct SearchResult {
  SearchResult();
  SearchResult(SearchResult&&);
  ~SearchResult();
  SearchResult& operator=(SearchResult&&);

  // Explicit copy only, since the `answerer_result` contains a log entry.
  // This should only be called if `answerer_result` is not populated with
  // a log entry yet, for example after initial search and before answering.
  SearchResult Clone();

  // Returns true if this search result is related to the given `other`
  // result returned by HistoryEmbeddingsService::Search (same session/query).
  bool IsContinuationOf(const SearchResult& other);

  // Gets the answer text from within the `answerer_result`.
  const std::string& AnswerText() const;

  // Finds the index in `scored_url_rows` that has the URL selected by the
  // `answerer_result`, indicating where the answer came from.
  size_t AnswerIndex() const;

  // Session ID to associate query with answers.
  std::string session_id;

  // Keep context for search parameters requested, to make logging easier.
  std::string query;
  std::optional<base::Time> time_range_start;
  size_t count = 0;

  // The actual search result data. Note that the size of this vector will
  // not necessarily match the above requested `count`.
  std::vector<ScoredUrlRow> scored_url_rows;

  // This may be empty for initial embeddings search results, as the answer
  // isn't ready yet. When the answerer finishes work, a second search
  // result is provided with this answer filled.
  AnswererResult answerer_result;
};

using SearchResultCallback = base::RepeatingCallback<void(SearchResult)>;

using QualityLogEntry =
    std::unique_ptr<optimization_guide::ModelQualityLogEntry>;

class HistoryEmbeddingsService : public KeyedService,
                                 public history::HistoryServiceObserver {
 public:
  // Number of low-order bits to use in session_id for sequence number.
  static constexpr uint64_t kSessionIdSequenceBits = 16;
  static constexpr uint64_t kSessionIdSequenceBitMask =
      (1 << kSessionIdSequenceBits) - 1;

  // `history_service` is never nullptr and must outlive `this`.
  // Storage uses its `history_dir() location for the database.
  HistoryEmbeddingsService(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      history::HistoryService* history_service,
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      std::unique_ptr<Embedder> embedder,
      std::unique_ptr<Answerer> answerer,
      std::unique_ptr<IntentClassifier> intent_classifier);
  HistoryEmbeddingsService(const HistoryEmbeddingsService&) = delete;
  HistoryEmbeddingsService& operator=(const HistoryEmbeddingsService&) = delete;
  ~HistoryEmbeddingsService() override;

  // Identify if the given URL is eligible for history embeddings.
  bool IsEligible(const GURL& url);

  // Initiate async passage extraction from given host's main frame.
  // When extraction completes, the passages will be stored in the database
  // and then given to the callback.
  // Note: A `WeakDocumentPtr` is essentially a `WeakPtr<RenderFrameHost>`.
  void RetrievePassages(history::URLID url_id,
                        history::VisitID visit_id,
                        base::Time visit_time,
                        content::WeakDocumentPtr weak_render_frame_host);

  // Find top `count` URL visit info entries nearest given `query`. Pass results
  // to given `callback` when search completes. Search will be narrowed to a
  // time range if `time_range_start` is provided. In that case, the start of
  // the time range is inclusive and the end is unbounded. Practically, this can
  // be thought of as [start, now) but now isn't fixed. Virtual for testing.
  // The `callback` may be called back later with another search result
  // containing an answer. This two-phase result callback scheme lets callers
  // receive initial search results without having to wait longer for answers.
  // The `previous_search_result` may be nullptr to signal the beginning of a
  // completely new search session; if it is non-null and the session_id was
  // set, the new session_id is set based on the previous to indicate a
  // continuing search session. Returns a stub result that can be used to detect
  // if a later published SearchResult instance is related to this search.
  virtual SearchResult Search(SearchResult* previous_search_result,
                              std::string query,
                              std::optional<base::Time> time_range_start,
                              size_t count,
                              SearchResultCallback callback);

  // Weak `this` provider method.
  base::WeakPtr<HistoryEmbeddingsService> AsWeakPtr();

  // Submit quality logging data after user selects an item from search result.
  // Note, the `result` contains a log entry that will be consumed by this call.
  void SendQualityLog(SearchResult& result,
                      optimization_guide::proto::UserFeedback user_feedback,
                      std::set<size_t> selections,
                      size_t num_entered_characters,
                      bool from_omnibox_history_scope);

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class HistoryEmbeddingsBrowserTest;
  friend class HistoryEmbeddingsServicePublic;
  friend class ::HistoryEmbeddingsInteractiveTest;

  // A utility container to wrap anything that should be accessed on
  // the separate storage worker sequence.
  struct Storage {
    explicit Storage(const base::FilePath& storage_dir);

    // Associate the given metadata with this Storage instance. The storage is
    // not considered initialized until this metadata is supplied.
    void SetEmbedderMetadata(EmbedderMetadata metadata,
                             os_crypt_async::Encryptor encryptor);

    // Called on the worker sequence to persist passages and embeddings.
    void ProcessAndStorePassages(UrlPassages url_passages,
                                 std::vector<Embedding> passages_embeddings);

    // Runs search on worker sequence.
    std::vector<ScoredUrlRow> Search(
        base::WeakPtr<std::atomic<size_t>> weak_latest_query_id,
        size_t query_id,
        SearchParams search_params,
        Embedding query_embedding,
        std::optional<base::Time> time_range_start,
        size_t count);

    // Handles the History deletions on the worker thread.
    void HandleHistoryDeletions(bool for_all_history,
                                history::URLRows deleted_rows,
                                std::set<history::VisitID> deleted_visit_ids);

    // Targeted deletion for testing scenarios like model version change.
    void DeleteDataForTesting(bool delete_passages, bool delete_embeddings);

    // Gathers URL and passage data from the database where corresponding
    // embeddings are absent. This is used to rebuild the embeddings table
    // when the model changes.
    std::vector<UrlPassages> CollectPassagesWithoutEmbeddings();

    // Retrieves passages and embeddings from the database for use as a cache
    // to avoid recomputing embeddings that exist for identical passages.
    std::optional<UrlPassagesEmbeddings> GetUrlData(history::URLID url_id);

    // A VectorDatabase implementation that holds data in memory.
    VectorDatabaseInMemory vector_database;

    // The underlying SQL database for persistent storage.
    SqlDatabase sql_database;
  };

  // Called when the embedder metadata is available. Passes the metadata to
  // the internal storage.
  void OnEmbedderMetadataReady(EmbedderMetadata metadata);

  void OnOsCryptAsyncReady(EmbedderMetadata metadata,
                           os_crypt_async::Encryptor encryptor,
                           bool success);

  // This can be overridden to gate answer generation for some accounts.
  virtual bool IsAnswererUseAllowed() const;

  // This can be overridden to prepare a log entry that will then be filled
  // with data and sent on destruction. Default implementation returns null.
  virtual QualityLogEntry PrepareQualityLogEntry();

  // Called indirectly via `RetrievePassagesWithUrlData` when passage extraction
  // completes.
  void OnPassagesRetrieved(
      std::optional<UrlPassagesEmbeddings> existing_url_data,
      UrlPassages url_passages,
      std::vector<std::string> passages);

  // Invoked after the embeddings for `passages` has been computed.
  void OnPassagesEmbeddingsComputed(
      std::unordered_map<std::string, Embedding> embedding_cache,
      UrlPassages url_passages,
      std::vector<std::string> passages,
      std::vector<Embedding> embeddings,
      ComputeEmbeddingsStatus status);

  // Invoked after the embedding for the original search query has been
  // computed.
  void OnQueryEmbeddingComputed(SearchResultCallback callback,
                                SearchParams search_params,
                                SearchResult result,
                                std::vector<std::string> query_passages,
                                std::vector<Embedding> query_embedding,
                                ComputeEmbeddingsStatus status);

  // Finishes a search result by combining found data with additional data from
  // history database. Moves each ScoredUrl into a more complete structure with
  // a history URLRow. Omits any entries that don't have corresponding data in
  // the history database.
  void OnSearchCompleted(SearchResultCallback callback,
                         SearchResult result,
                         std::vector<ScoredUrlRow> scored_url_rows);

  // Calls `page_content_annotation_service_` to determine whether the passage
  // of each ScoredUrl should be shown to the user.
  void DeterminePassageVisibility(SearchResultCallback callback,
                                  SearchResult result,
                                  std::vector<ScoredUrlRow> scored_url_rows);

  // Called after `page_content_annotation_service_` has determined visibility
  // for the passage of each ScoredUrl. This will filter `scored_urls` to only
  // contain entries that can be shown to the user.
  void OnPassageVisibilityCalculated(
      SearchResultCallback callback,
      SearchResult result,
      std::vector<ScoredUrlRow> scored_url_rows,
      const std::vector<page_content_annotations::BatchAnnotationResult>&
          annotation_results);

  // Called on main sequence after the history worker thread finalizes
  // the initial search result with URL rows. Calls the `callback` and
  // then proceeds to intent check and v2 answer generation if needed.
  void OnPrimarySearchResultReady(SearchResultCallback callback,
                                  SearchResult result);

  // Invoked after the intent classifier computes query answerability.
  void OnQueryIntentComputed(SearchResultCallback callback,
                             SearchResult result,
                             ComputeIntentStatus status,
                             bool query_is_answerable);

  // Called after the answerer finishes computing an answer. Combines
  // the `answer_result` into `search_result` and invokes `callback`
  // with new search result complete with answer.
  void OnAnswerComputed(SearchResultCallback callback,
                        SearchResult search_result,
                        AnswererResult answerer_result);

  // Rebuild absent embeddings from source passages.
  void RebuildAbsentEmbeddings(std::vector<UrlPassages> all_url_passages);

  // This continues with passage extraction after any existing data is fetched
  // for the same `url_id`.
  void RetrievePassagesWithUrlData(
      history::URLID url_id,
      history::VisitID visit_id,
      base::Time visit_time,
      content::WeakDocumentPtr weak_render_frame_host,
      base::Time time_before_database_access,
      std::optional<UrlPassagesEmbeddings> existing_url_data);

  // Returns true if query should be filtered. If false, then `search_params`
  // will have its query_terms set.
  bool QueryIsFiltered(const std::string& raw_query,
                       SearchParams& search_params) const;

  raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;

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

  // Used to determine whether a page should be excluded from history
  // embeddings.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  // Tracks the observed history service, for cleanup.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // The embedder used to compute embeddings.
  std::unique_ptr<Embedder> embedder_;

  // The answerer used to answer queries with context. May be nullptr if
  // the kHistoryEmbeddingsAnswers feature is disabled.
  std::unique_ptr<Answerer> answerer_;

  // The intent classifier used to determine query intent and answerability.
  std::unique_ptr<IntentClassifier> intent_classifier_;

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

  base::CallbackListSubscription subscription_;

  base::WeakPtrFactory<std::atomic<size_t>> query_id_weak_ptr_factory_;

  base::WeakPtrFactory<HistoryEmbeddingsService> weak_ptr_factory_;
};

// This corresponds to UMA histogram enum `EmbeddingsQueryFiltered`
// in tools/metrics/histograms/metadata/history/enums.xml
enum class QueryFiltered {
  NOT_FILTERED,
  FILTERED_NOT_ASCII,
  FILTERED_PHRASE_MATCH,
  FILTERED_TERM_MATCH,
  FILTERED_ONE_WORD_HASH_MATCH,
  FILTERED_TWO_WORD_HASH_MATCH,

  // These enum values are logged in UMA. Do not reuse or skip any values.
  // The order doesn't need to be chronological, but keep identities stable.
  ENUM_COUNT,
};

// Record UMA histogram with query filter status.
void RecordQueryFiltered(QueryFiltered status);

// This corresponds to UMA histogram enum `EmbeddingsExtractionCancelled`
// in tools/metrics/histograms/metadata/history/enums.xml
enum class ExtractionCancelled {
  UNKNOWN = 0,
  TAB_HELPER_DID_FINISH_LOAD = 1,
  TAB_HELPER_EXTRACT_PASSAGES_URL = 2,
  TAB_HELPER_EXTRACT_PASSAGES_RESCHEDULE = 3,
  TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_RESULTS = 4,
  TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_TIME = 5,
  TAB_HELPER_EXTRACT_PASSAGES_WITH_HISTORY_DATA_GUID = 6,
  SERVICE_RETRIEVE_PASSAGES = 7,
  SERVICE_RETRIEVE_PASSAGES_WITH_URL_DATA = 8,

  // These enum values are logged in UMA. Do not reuse or skip any values.
  // The order doesn't need to be chronological, but keep identities stable.
  ENUM_COUNT,
};

// Record UMA histogram with cancellation reason when extraction,
// embedding, etc. is cancelled before completion and storage.
void RecordExtractionCancelled(ExtractionCancelled reason);

// Hash function used for query filtering.
uint32_t HashString(std::string_view str);

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
