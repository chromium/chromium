// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_FEATURES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_FEATURES_H_

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history_clusters {
namespace features {

// Params & helpers functions

// Enables configuring the on-device clustering backend.
extern const base::Feature kOnDeviceClustering;

// Uses an in-memory cache that stores engagement score.
extern const base::Feature kUseEngagementScoreCache;

// Splits clustering task into smaller batches.
extern const base::Feature kSplitClusteringTasksToSmallerBatches;

// Specifies various blocklists for on-device clustering backend.
extern const base::Feature kOnDeviceClusteringBlocklists;

}  // namespace features
}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_ON_DEVICE_CLUSTERING_FEATURES_H_
