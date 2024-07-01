// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history_embeddings {

BASE_DECLARE_FEATURE(kHistoryEmbeddings);

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

// Specifies whether to use the ML Answerer (if false, the mock is used).
extern const base::FeatureParam<bool> kUseMlAnswerer;

// Specifies whether to use the ML Embedder to embed passages and queries.
extern const base::FeatureParam<bool> kUseMlEmbedder;

// Whether history embedding results should be shown in the omnibox outside of
// the '@history' scope.
extern const base::FeatureParam<bool> kOmniboxUnscoped;

// The maximum number of embeddings to submit to the primary (ML) embedder
// in a single batch via the scheduling embedder.
extern const base::FeatureParam<int> kScheduledEmbeddingsMax;

// Whether quality logging data should be sent.
extern const base::FeatureParam<bool> kSendQualityLog;

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

bool IsHistoryEmbeddingEnabled();

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_
