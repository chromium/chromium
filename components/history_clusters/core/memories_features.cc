// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_features.h"

#include "base/metrics/field_trial_params.h"

namespace history_clusters {

namespace {

const base::FeatureParam<std::string> kRemoteModelEndpoint{
    &kRemoteModelForDebugging, "MemoriesRemoteModelEndpoint", ""};

}  // namespace

GURL RemoteModelEndpoint() {
  return GURL(kRemoteModelEndpoint.Get());
}

const base::FeatureParam<std::string> kRemoteModelEndpointExperimentName{
    &kRemoteModelForDebugging, "MemoriesRemoteModelEndpointExperimentName", ""};

const base::FeatureParam<bool> kPersistContextAnnotationsInHistoryDb{
    &kMemories, "MemoriesPersistContextAnnotationsInHistoryDb", false};

const base::FeatureParam<int> kMaxVisitsToCluster{
    &kMemories, "MemoriesMaxVisitsToCluster", 10};

const base::FeatureParam<int> kMaxDaysToCluster{&kMemories,
                                                "MemoriesMaxDaysToCluster", 9};

const base::FeatureParam<bool> kPersistClustersInHistoryDb{
    &kMemories, "MemoriesPersistClustersInHistoryDb", false};

const base::Feature kMemories{"Memories", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDebug{"MemoriesDebug", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoteModelForDebugging{"MemoriesRemoteModelForDebugging",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace history_clusters
