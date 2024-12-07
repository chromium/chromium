// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_FEATURES_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_FEATURES_H_

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace passage_embeddings {

// The number of threads to use for embeddings generation with
// mojom::PassagePriority::kUserInitiated.
extern const base::FeatureParam<int> kUserInitiatedPriorityNumThreads;

// The number of threads to use for embeddings generation with
// mojom::PassagePriority::kPassive.
extern const base::FeatureParam<int> kPassivePriorityNumThreads;

// The size of the cache the embedder uses to limit execution on the same
// passage.
extern const base::FeatureParam<int> kEmbedderCacheSize;

// The amount of time the passage embedder will idle for before being torn down
// to reduce memory usage.
extern const base::FeatureParam<base::TimeDelta> kEmbedderTimeout;

// The amount of time the passage embeddings service will idle for before being
// torn down to reduce memory usage.
extern const base::FeatureParam<base::TimeDelta> kEmbeddingsServiceTimeout;

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_FEATURES_H_
