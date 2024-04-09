// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history_embeddings {

BASE_DECLARE_FEATURE(kHistoryEmbeddings);

// Specifies the `max_words_per_aggregate_passage` parameter for the
// DocumentChunker passage extraction algorithm. A passage from a single
// node can exceed this maximum, but aggregation keeps within the limit.
extern const base::FeatureParam<int>
    kPassageExtractionMaxWordsPerAggregatePassage;

// Specifies the number of best matching items to take from the search.
extern const base::FeatureParam<int> kSearchResultItemCount;

// Specifies whether to accelerate keyword mode entry when @ is entered
// followed by the first letter of a starter pack keyword.
extern const base::FeatureParam<bool> kAtKeywordAcceleration;

// Specifies the content visibility threshold that can be shown to the user.
extern const base::FeatureParam<double> kContentVisibilityThreshold;

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_FEATURES_H_
