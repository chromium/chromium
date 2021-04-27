// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_features.h"

#include "base/metrics/field_trial_params.h"

namespace history_clusters {

GURL RemoteModelEndpointForDebugging() {
  return GURL(base::GetFieldTrialParamValueByFeature(
      kRemoteModelForDebugging, "MemoriesRemoteModelEndpoint"));
}

bool StoreVisitsInHistoryDb() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMemories, "MemoriesStoreVisitsInHistoryDb", false);
}

int MaxVisitsToCluster() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kMemories, "MemoriesMaxVisitsToCluster", 10);
}

// Enables the Chrome Memories history clustering feature.
const base::Feature kMemories{"Memories", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables debug info; e.g. shows visit metadata on chrome://history entries.
const base::Feature kDebug{"MemoriesDebug", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using a remote model endpoint for Memories clustering for debugging
// purposes. This should not be ever enabled in production.
const base::Feature kRemoteModelForDebugging{"MemoriesRemoteModelForDebugging",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace history_clusters
