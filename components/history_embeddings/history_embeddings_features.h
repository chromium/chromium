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
extern const base::FeatureParam<bool> kShowSourcePassages;

// Number of milliseconds to wait after `DidFinishLoad` before extracting
// passages, computing and storing their embeddings, etc. Note, the
// extraction will only begin if no tabs are loading. If any are
// loading then the delay is applied again to reschedule extraction.
// To avoid CPU churn from rescheduling, keep this value well above zero.
extern const base::FeatureParam<int> kPassageExtractionDelay;

// Specifies the `max_words_per_aggregate_passage` parameter for the
// DocumentChunker passage extraction algorithm. A passage from a single
// node can exceed this maximum, but aggregation keeps within the limit.
extern const base::FeatureParam<int>
    kPassageExtractionMaxWordsPerAggregatePassage;

// The minimum number of words a query or passage must have in order to be
// included in similarity search.
extern const base::FeatureParam<int> kSearchQueryMinimumWordCount;
extern const base::FeatureParam<int> kSearchPassageMinimumWordCount;

// The minimum number of words to gather from several passages used as
// context for the Answerer. Top passages will be included until the sum
// of word counts meets this minimum.
extern const base::FeatureParam<int> kContextPassagesMinimumWordCount;

// Specifies the number of best matching items to take from the search.
extern const base::FeatureParam<int> kSearchResultItemCount;

// Specifies whether to accelerate keyword mode entry when @ is entered
// followed by the first letter of a starter pack keyword.
extern const base::FeatureParam<bool> kAtKeywordAcceleration;

// Specifies the content visibility threshold that can be shown to the user.
// This is for safety filtering.
extern const base::FeatureParam<double> kContentVisibilityThreshold;

// Specifies the similarity score threshold that embeddings must pass in order
// for their results to be shown to the user. This is for general search scoring
// and result inclusion.
extern const base::FeatureParam<double> kSearchScoreThreshold;

// Specifies whether to use the intent classifier to gate answer generation.
extern const base::FeatureParam<bool> kEnableIntentClassifier;

// Specifies whether to use the ML intent classifier (if false, the mock is
// used).
extern const base::FeatureParam<bool> kUseMlIntentClassifier;

// Specifies the delay in milliseconds to use for the mock intent classifier for
// local development.
extern const base::FeatureParam<int> kMockIntentClassifierDelayMS;

// Specifies whether to use the ML Answerer (if false, the mock is used).
extern const base::FeatureParam<bool> kUseMlAnswerer;

// Specifies the min score for generated answer from the ML answerer.
extern const base::FeatureParam<double> kMlAnswererMinScore;

// Specifies the delay in milliseconds to use for the mock answerer for local
// development.
extern const base::FeatureParam<int> kMockAnswererDelayMS;

// Specifies whether to show images in results for search results on the
// chrome://history page.
extern const base::FeatureParam<bool> kEnableImagesForResults;

// Whether history embedding results should be shown in the omnibox when in the
// '@history' scope.
extern const base::FeatureParam<bool> kOmniboxScoped;

// Whether history embedding results should be shown in the omnibox when not in
// the '@history' scope. If true, behaves as if `kOmniboxScoped` is also true.
extern const base::FeatureParam<bool> kOmniboxUnscoped;

// The maximum number of embeddings to submit to the primary (ML) embedder
// in a single batch via the scheduling embedder.
extern const base::FeatureParam<int> kScheduledEmbeddingsMax;

// Whether quality logging data should be sent.
extern const base::FeatureParam<bool> kSendQualityLog;
extern const base::FeatureParam<bool> kSendQualityLogV2;

// The number of threads to use for embeddings generation. A value of -1 means
// to use the default number of threads.
extern const base::FeatureParam<int> kEmbedderNumThreads;

// The size of the cache the embedder uses to limit execution on the same
// passage.
extern const base::FeatureParam<int> kEmbedderCacheSize;

// The max number of passages that can be extracted from a page. Passages over
// this limit will be dropped by passage extraction.
extern const base::FeatureParam<int> kMaxPassagesPerPage;

// These parameters control deletion and rebuilding of the embeddings
// database. If `kDeleteEmbeddings` is true, the embeddings table will
// be cleared on startup, effectively simulating a model version change.
// If `kRebuildEmbeddings` is true (the default) then any rows in
// the passages table without a corresponding row in the embeddings
// table (keyed on url_id) will be queued for reprocessing by the embedder.
extern const base::FeatureParam<bool> kDeleteEmbeddings;
extern const base::FeatureParam<bool> kRebuildEmbeddings;

// When true (the default), passages and embeddings from the database are
// used as a perfect cache to avoid re-embedding any passages that already
// exist in a given url_id's stored data. This reduces embedding workload
// to the minimum necessary for new passages, with no redundant recomputes.
extern const base::FeatureParam<bool> kUseDatabaseBeforeEmbedder;

// Whether to enable the URL filter to skip blocked URLs to improve performance.
extern const base::FeatureParam<bool> kUseUrlFilter;

// The amount of time in seconds that the passage embeddings service will idle
// for before being torn down to reduce memory usage.
extern const base::FeatureParam<base::TimeDelta> kEmbeddingsServiceTimeout;

// Specifies whether the history clusters side panel UI also searches and shows
// history embeddings.
extern const base::FeatureParam<bool> kEnableSidePanel;

// The maximum number of URLs to use when building context for answerer.
extern const base::FeatureParam<int> kMaxAnswererContextUrlCount;

// These control score boosting from passage text word matching.
extern const base::FeatureParam<double> kWordMatchMinEmbeddingScore;
extern const base::FeatureParam<int> kWordMatchMinTermLength;
extern const base::FeatureParam<double> kWordMatchScoreBoostFactor;
extern const base::FeatureParam<int> kWordMatchLimit;
extern const base::FeatureParam<int> kWordMatchSmoothingFactor;
extern const base::FeatureParam<int> kWordMatchMaxTermCount;

// Whether the history embeddings feature is enabled. This only checks if the
// feature flags are enabled and does not check the user's opt-in preference.
// See chrome/browser/history_embeddings/history_embeddings_utils.h.
bool IsHistoryEmbeddingsEnabled();

// Whether the history answers feature is enabled.
bool IsHistoryEmbeddingsAnswersEnabled();

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_
