// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_FEATURES_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_FEATURES_H_

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace passage_embeddings {

// Used for creating `EmbedderService` and passage extraction in tab helper.
// It also hold the feature params used by `PassageEmbeddingsService` to run the
// passage embedder, but does not switch it off.
BASE_DECLARE_FEATURE(kPassageEmbedder);

// The number of threads to use for embeddings generation with
// mojom::PassagePriority::kUserInitiated.
extern const base::FeatureParam<int> kUserInitiatedPriorityNumThreads;

// The number of threads to use for embeddings generation with
// mojom::PassagePriority::kUrgent.
extern const base::FeatureParam<int> kUrgentPriorityNumThreads;

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

// The amount of time to wait after `DidFinishLoad` before extracting passages
// and computing their embeddings. Note, the extraction will only begin if no
// tabs are loading. If any are loading then the delay is applied again to
// reschedule extraction.
extern const base::FeatureParam<base::TimeDelta> kPassageExtractionDelay;

// Specifies the `max_words_per_aggregate_passage` parameter for the
// DocumentChunker passage extraction algorithm. A passage from a single
// node can exceed this maximum, but aggregation keeps within the limit.
extern const base::FeatureParam<int> kMaxWordsPerAggregatePassage;

// Specifies the `max_passages` parameter for the DocumentChunker passage
// extraction algorithm. Passages over this limit will be dropped by passage
// extraction.
extern const base::FeatureParam<int> kMaxPassagesPerPage;

// Specifies the `min_words_per_passage` parameter for the DocumentChunker
// passage extraction algorithm.
extern const base::FeatureParam<int> kMinWordsPerPassage;

// Specifies whether GPU execution is allowed for execution if there is a GPU
// for the device.
extern const base::FeatureParam<bool> kAllowGpuExecution;

// Maximum number of jobs the SchedulingEmbedder will hold at a time. This
// acts as a hard cap to limit memory usage, which may be needed
// especially when waiting for performance scenario.
extern const base::FeatureParam<int> kSchedulerMaxJobs;

// Maximum number of embeddings the SchedulingEmbedder submits for a single
// batch of work.
extern const base::FeatureParam<int> kSchedulerMaxBatchSize;

// Whether to wait for a suitable performance scenario before submitting
// work to the embedder.
extern const base::FeatureParam<bool> kUsePerformanceScenario;

// Whether to use the background passage embedder (vs. immediate passage
// embedder) for computing WebContents passage embeddings.
extern const base::FeatureParam<bool> kUseBackgroundPassageEmbedder;

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_FEATURES_H_
