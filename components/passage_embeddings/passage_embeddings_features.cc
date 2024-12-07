// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/passage_embeddings_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace passage_embeddings {

// Exists to hold the feature params used to run the passage embedder.
BASE_FEATURE(kPassageEmbeddings,
             "PassageEmbeddings",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kUserInitiatedPriorityNumThreads(
    &kPassageEmbeddings,
    "UserInitiatedPriorityNumThreads",
    4);

const base::FeatureParam<int> kPassivePriorityNumThreads(
    &kPassageEmbeddings,
    "PassivePriorityNumThreads",
    1);

const base::FeatureParam<int> kEmbedderCacheSize(&kPassageEmbeddings,
                                                 "EmbedderCacheSize",
                                                 1000);

const base::FeatureParam<base::TimeDelta> kEmbedderTimeout(&kPassageEmbeddings,
                                                           "EmbedderTimeout",
                                                           base::Seconds(60));

const base::FeatureParam<base::TimeDelta> kEmbeddingsServiceTimeout(
    &kPassageEmbeddings,
    "EmbeddingsServiceTimeout",
    base::Seconds(60));

}  // namespace passage_embeddings
