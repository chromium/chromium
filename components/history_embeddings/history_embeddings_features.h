// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history_embeddings {

// This is the main feature switch for history embeddings search, and when it is
// disabled, answering functionality will not be available either.
BASE_DECLARE_FEATURE(kHistoryEmbeddings);

// This feature specifies whether to answer queries using an answerer; it can be
// considered a toggle for v2 answering functionality. Parameters are all kept
// under the primary kHistoryEmbeddings feature. The kHistoryEmbeddingsAnswers
// feature state is not applicable if kHistoryEmbeddings is disabled.
BASE_DECLARE_FEATURE(kHistoryEmbeddingsAnswers);

// Displays source passages in the UI on chrome://history for debug purposes.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kShowSourcePassages;

// Number of milliseconds to wait after `DidFinishLoad` before extracting
// passages, computing and storing their embeddings, etc. Note, the
// extraction will only begin if no tabs are loading. If any are
// loading then the delay is applied again to reschedule extraction.
// To avoid CPU churn from rescheduling, keep this value well above zero.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kPassageExtractionDelay;

// Specifies the `max_words_per_aggregate_passage` parameter for the
// DocumentChunker passage extraction algorithm. A passage from a single
// node can exceed this maximum, but aggregation keeps within the limit.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int>
    kPassageExtractionMaxWordsPerAggregatePassage;

// The minimum number of words a query or passage must have in order to be
// included in similarity search.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kSearchQueryMinimumWordCount;
extern const base::FeatureParam<int> kSearchPassageMinimumWordCount;

// The minimum number of words to gather from several passages used as
// context for the Answerer. Top passages will be included until the sum
// of word counts meets this minimum.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kContextPassagesMinimumWordCount;

// Specifies the number of best matching items to take from the search.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kSearchResultItemCount;

// Specifies whether to accelerate keyword mode entry when @ is entered
// followed by the first letter of a starter pack keyword.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kAtKeywordAcceleration;

// Specifies the content visibility threshold that can be shown to the user.
// This is for safety filtering.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<double> kContentVisibilityThreshold;

// Specifies the similarity score threshold that embeddings must pass in order
// for their results to be shown to the user. This is for general search scoring
// and result inclusion.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<double> kSearchScoreThreshold;

// Specifies whether to use the intent classifier to gate answer generation.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kEnableIntentClassifier;

// Specifies whether to use the ML intent classifier (if false, the mock is
// used).
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kUseMlIntentClassifier;

// Specifies the delay in milliseconds to use for the mock intent classifier for
// local development.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kMockIntentClassifierDelayMS;

// Specifies whether to use the ML Answerer (if false, the mock is used).
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kUseMlAnswerer;

// Specifies the min score for generated answer from the ML answerer.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<double> kMlAnswererMinScore;

// Specifies the delay in milliseconds to use for the mock answerer for local
// development.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kMockAnswererDelayMS;

// Specifies the answer status to use for the mock answerer for local
// development.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kMockAnswererStatus;

// This can be used to bypass IsAnswererUseAllowed checks. It's necessary for
// testing and development but should remain false in real configurations.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kForceAnswererUseAllowed;

// Specifies whether to show images in results for search results on the
// chrome://history page.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kEnableImagesForResults;

// Whether history embedding results should be shown in the omnibox when in the
// '@history' scope.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kOmniboxScoped;

// Whether history embedding results should be shown in the omnibox when not in
// the '@history' scope. If true, behaves as if `kOmniboxScoped` is also true.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kOmniboxUnscoped;

// Whether history embedding answers should be shown in the omnibox when in the
// '@history' scope. No-op if `kOmniboxScoped` is false.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kAnswersInOmniboxScoped;

// The maximum number of embeddings to submit to the primary (ML) embedder
// in a single batch via the scheduling embedder.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kScheduledEmbeddingsMax;

// Whether quality logging data should be sent.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kSendQualityLog;
extern const base::FeatureParam<bool> kSendQualityLogV2;

// The number of threads to use for embeddings generation. A value of -1 means
// to use the default number of threads.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kEmbedderNumThreads;

// The size of the cache the embedder uses to limit execution on the same
// passage.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kEmbedderCacheSize;

// The max number of passages that can be extracted from a page. Passages over
// this limit will be dropped by passage extraction.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kMaxPassagesPerPage;

// These parameters control deletion and rebuilding of the embeddings
// database. If `kDeleteEmbeddings` is true, the embeddings table will
// be cleared on startup, effectively simulating a model version change.
// If `kRebuildEmbeddings` is true (the default) then any rows in
// the passages table without a corresponding row in the embeddings
// table (keyed on url_id) will be queued for reprocessing by the embedder.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kDeleteEmbeddings;
extern const base::FeatureParam<bool> kRebuildEmbeddings;

// When true (the default), passages and embeddings from the database are
// used as a perfect cache to avoid re-embedding any passages that already
// exist in a given url_id's stored data. This reduces embedding workload
// to the minimum necessary for new passages, with no redundant recomputes.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kUseDatabaseBeforeEmbedder;

// Whether to enable the URL filter to skip blocked URLs to improve performance.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kUseUrlFilter;

// The amount of time in seconds that the passage embeddings service will idle
// for before being torn down to reduce memory usage.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<base::TimeDelta> kEmbeddingsServiceTimeout;

// Specifies whether the history clusters side panel UI also searches and shows
// history embeddings.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kEnableSidePanel;

// Specifies whether history embedding results should show just the hostname of
// the result's URL.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kTrimAfterHostInResults;

// The maximum number of URLs to use when building context for answerer.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<int> kMaxAnswererContextUrlCount;

// These control score boosting from passage text word matching.
// See comments for `SearchParams` struct for more details about each value.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<double> kWordMatchMinEmbeddingScore;
extern const base::FeatureParam<int> kWordMatchMinTermLength;
extern const base::FeatureParam<double> kWordMatchScoreBoostFactor;
extern const base::FeatureParam<int> kWordMatchLimit;
extern const base::FeatureParam<int> kWordMatchSmoothingFactor;
extern const base::FeatureParam<int> kWordMatchMaxTermCount;
extern const base::FeatureParam<double> kWordMatchRequiredTermRatio;

// Whether to include scroll to text fragment directives with answer citations.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kScrollTagsEnabled;

// Whether to erase non-ASCII characters from passages before sending them to
// the ML embedder. When false, passages are embedded without modification but
// then will be skipped during search. When true, passages are embedded with
// non-ASCII characters removed, but are then included in search.
// Deprecated. Use `FeatureParameters` struct instead.
extern const base::FeatureParam<bool> kEraseNonAsciiCharacters;

// Whether the history embeddings feature is enabled. This only checks if the
// feature flags are enabled and does not check the user's opt-in preference.
// See chrome/browser/history_embeddings/history_embeddings_utils.h.
bool IsHistoryEmbeddingsEnabled();

// Whether the history answers feature is enabled.
bool IsHistoryEmbeddingsAnswersEnabled();

// Contains feature configuration state. Can be set using Finch or overridden
// for testing. Prefer to use this struct instead of above feature parameters
// directly so as to reduce dependency on Finch.
struct FeatureParameters {
  explicit FeatureParameters(bool load_finch);
  FeatureParameters(const FeatureParameters&);
  FeatureParameters(FeatureParameters&&);
  FeatureParameters& operator=(const FeatureParameters&);
  FeatureParameters& operator=(FeatureParameters&&);

  // Displays source passages in the UI on chrome://history for debug purposes.
  bool show_source_passages = false;

  // Number of milliseconds to wait after `DidFinishLoad` before extracting
  // passages, computing and storing their embeddings, etc. Note, the
  // extraction will only begin if no tabs are loading. If any are
  // loading then the delay is applied again to reschedule extraction.
  // To avoid CPU churn from rescheduling, keep this value well above zero.
  int passage_extraction_delay = 5000;

  // Specifies the `max_words_per_aggregate_passage` parameter for the
  // DocumentChunker passage extraction algorithm. A passage from a single
  // node can exceed this maximum, but aggregation keeps within the limit.
  int passage_extraction_max_words_per_aggregate_passage = 200;

  // The minimum number of words a query or passage must have in order to be
  // included in similarity search.
  int search_query_minimum_word_count = 2;
  int search_passage_minimum_word_count = 5;

  // The minimum number of words to gather from several passages used as
  // context for the Answerer. Top passages will be included until the sum
  // of word counts meets this minimum.
  int context_passages_minimum_word_count = 1000;

  // Specifies the number of best matching items to take from the search.
  int search_result_item_count = 3;

  // Specifies whether to accelerate keyword mode entry when @ is entered
  // followed by the first letter of a starter pack keyword.
  bool at_keyword_acceleration = false;

  // Specifies the content visibility threshold that can be shown to the user.
  // This is for safety filtering.
  double content_visibility_threshold = 0;

  // Specifies the similarity score threshold that embeddings must pass in order
  // for their results to be shown to the user. This is for general search
  // scoring and result inclusion.
  double search_score_threshold = -1;

  // Specifies whether to use the intent classifier to gate answer generation.
  bool enable_intent_classifier = true;

  // Specifies whether to use the ML intent classifier (if false, the mock is
  // used).
  bool use_ml_intent_classifier = false;

  // Specifies whether to output scores (Decoding output is skipped when
  // enabled).
  bool enable_ml_intent_classifier_score = false;

  // Specifies the delay in milliseconds to use for the mock intent classifier
  // for local development.
  int mock_intent_classifier_delay_ms = 0;

  // Specifies whether to use the ML Answerer (if false, the mock is used).
  bool use_ml_answerer = false;

  // Specifies the min score for generated answer from the ML answerer.
  double ml_answerer_min_score = 0.5;

  // Specifies the delay in milliseconds to use for the mock answerer for local
  // development.
  int mock_answerer_delay_ms = 0;

  // Specifies the answer status to use for the mock answerer for local
  // development.
  int mock_answerer_status = 2;

  // This can be used to bypass IsAnswererUseAllowed checks. It's necessary for
  // testing and development but should remain false in real configurations.
  bool force_answerer_use_allowed = false;

  // Specifies whether to show images in results for search results on the
  // chrome://history page.
  bool enable_images_for_results = false;

  // Whether history embedding results should be shown in the omnibox when in
  // the '@history' scope.
  bool omnibox_scoped = true;

  // Whether history embedding results should be shown in the omnibox when not
  // in the '@history' scope. If true, behaves as if `kOmniboxScoped` is also
  // true.
  bool omnibox_unscoped = false;

  // Whether history embedding answers should be shown in the omnibox when in
  // the '@history' scope. No-op if `kOmniboxScoped` is false.
  bool answers_in_omnibox_scoped = false;

  // The maximum number of embeddings to submit to the primary (ML) embedder
  // in a single batch via the scheduling embedder.
  int scheduled_embeddings_max = 1;

  // Whether quality logging data should be sent.
  bool send_quality_log = false;
  bool send_quality_log_v2 = false;

  // The number of threads to use for embeddings generation. A value of -1 means
  // to use the default number of threads.
  int embedder_num_threads = 4;

  // The size of the cache the embedder uses to limit execution on the same
  // passage.
  int embedder_cache_size = 1000;

  // The max number of passages that can be extracted from a page. Passages over
  // this limit will be dropped by passage extraction.
  int max_passages_per_page = 30;

  // These parameters control deletion and rebuilding of the embeddings
  // database. If `kDeleteEmbeddings` is true, the embeddings table will
  // be cleared on startup, effectively simulating a model version change.
  // If `kRebuildEmbeddings` is true (the default) then any rows in
  // the passages table without a corresponding row in the embeddings
  // table (keyed on url_id) will be queued for reprocessing by the embedder.
  bool delete_embeddings = false;
  bool rebuild_embeddings = true;

  // When true (the default), passages and embeddings from the database are
  // used as a perfect cache to avoid re-embedding any passages that already
  // exist in a given url_id's stored data. This reduces embedding workload
  // to the minimum necessary for new passages, with no redundant recomputes.
  bool use_database_before_embedder = true;

  // Whether to enable the URL filter to skip blocked URLs to improve
  // performance.
  bool use_url_filter = false;

  // The amount of time in seconds that the passage embeddings service will idle
  // for before being torn down to reduce memory usage.
  base::TimeDelta embeddings_service_timeout = base::Seconds(60);

  // Specifies whether the history clusters side panel UI also searches and
  // shows history embeddings.
  bool enable_side_panel = false;

  // Specifies whether history embedding results should show just the hostname
  // of the result's URL.
  bool trim_after_host_in_results = false;

  // The maximum number of URLs to use when building context for answerer.
  int max_answerer_context_url_count = 1;

  // These control score boosting from passage text word matching.
  // See comments for `SearchParams` struct for more details about each value.
  double word_match_min_embedding_score = 1.0;
  int word_match_min_term_length = 3;
  double word_match_score_boost_factor = 0.2;
  int word_match_limit = 5;
  int word_match_smoothing_factor = 1;
  int word_match_max_term_count = 3;
  double word_match_required_term_ratio = 1.0;

  // Whether to include scroll to text fragment directives with answer
  // citations.
  bool scroll_tags_enabled = false;

  // Whether to erase non-ASCII characters from passages before sending them to
  // the ML embedder. When false, passages are embedded without modification but
  // then will be skipped during search. When true, passages are embedded with
  // non-ASCII characters removed, but are then included in search.
  bool erase_non_ascii_characters = false;
};

// Use this to get the feature parameter configuration. This is immutable
// when running the browser.
const FeatureParameters& GetFeatureParameters();

// Use this to set the feature parameter configuration. This can only be
// done in tests.
void SetFeatureParametersForTesting(FeatureParameters parameters);

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_
