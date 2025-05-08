// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embeddings_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace passage_embeddings {

BASE_FEATURE(kPassageEmbedder,
             "PassageEmbedder",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kUserInitiatedPriorityNumThreads(
    &kPassageEmbedder,
    "UserInitiatedPriorityNumThreads",
    4);

const base::FeatureParam<int>
    kUrgentPriorityNumThreads(&kPassageEmbedder, "UrgentPriorityNumThreads", 4);

const base::FeatureParam<int> kPassivePriorityNumThreads(
    &kPassageEmbedder,
    "PassivePriorityNumThreads",
    1);

const base::FeatureParam<int> kEmbedderCacheSize(&kPassageEmbedder,
                                                 "EmbedderCacheSize",
                                                 1000);

const base::FeatureParam<base::TimeDelta> kEmbedderTimeout(&kPassageEmbedder,
                                                           "EmbedderTimeout",
                                                           base::Seconds(60));

const base::FeatureParam<base::TimeDelta> kEmbeddingsServiceTimeout(
    &kPassageEmbedder,
    "EmbeddingsServiceTimeout",
    base::Seconds(60));

const base::FeatureParam<base::TimeDelta> kPassageExtractionDelay(
    &kPassageEmbedder,
    "PassageExtractionDelay",
    base::Seconds(5));

const base::FeatureParam<int> kMaxWordsPerAggregatePassage(
    &kPassageEmbedder,
    "MaxWordsPerAggregatePassage",
    200);

const base::FeatureParam<int> kMaxPassagesPerPage(&kPassageEmbedder,
                                                  "MaxPassagesPerPage",
                                                  30);

const base::FeatureParam<int> kMinWordsPerPassage(&kPassageEmbedder,
                                                  "MinWordsPerPassage",
                                                  5);

const base::FeatureParam<bool> kAllowGpuExecution(&kPassageEmbedder,
                                                  "AllowGpuExecution",
                                                  false);

const base::FeatureParam<int> kSchedulerMaxJobs(&kPassageEmbedder,
                                                "SchedulerMaxJobs",
                                                64);

const base::FeatureParam<int> kSchedulerMaxBatchSize(&kPassageEmbedder,
                                                     "SchedulerMaxBatchSize",
                                                     1);

const base::FeatureParam<bool> kUsePerformanceScenario(&kPassageEmbedder,
                                                       "UsePerformanceScenario",
                                                       false);

const base::FeatureParam<bool> kUseBackgroundPassageEmbedder(
    &kPassageEmbedder,
    "UseBackgroundPassageEmbedder",
    false);

}  // namespace passage_embeddings
