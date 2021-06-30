// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_features.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace history_clusters {

namespace {

constexpr auto enabled_by_default_desktop_only =
#if defined(OS_ANDROID) || defined(OS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

const base::FeatureParam<std::string> kRemoteModelEndpoint{
    &kRemoteModelForDebugging, "MemoriesRemoteModelEndpoint", ""};

}  // namespace

GURL RemoteModelEndpoint() {
  return GURL(kRemoteModelEndpoint.Get());
}

const base::FeatureParam<std::string> kRemoteModelEndpointExperimentName{
    &kMemories, "MemoriesExperimentName", ""};

const base::FeatureParam<int> kMaxVisitsToCluster{
    &kMemories, "MemoriesMaxVisitsToCluster", 1000};

const base::FeatureParam<int> kMaxDaysToCluster{&kMemories,
                                                "MemoriesMaxDaysToCluster", 9};

const base::FeatureParam<bool> kPersistClustersInHistoryDb{
    &kMemories, "MemoriesPersistClustersInHistoryDb", false};

const base::FeatureParam<bool> kUseOnDeviceClusteringBackend{
    &kMemories, "MemoriesOnDeviceClusteringBackend", true};

const base::Feature kMemories{"Memories", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDebug{"MemoriesDebug", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoteModelForDebugging{"MemoriesRemoteModelForDebugging",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPersistContextAnnotationsInHistoryDb{
    "MemoriesPersistContextAnnotationsInHistoryDb",
    enabled_by_default_desktop_only};

}  // namespace history_clusters
