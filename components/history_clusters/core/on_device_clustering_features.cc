// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_features.h"

#include <algorithm>
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"

namespace history_clusters {
namespace features {

BASE_FEATURE(kOnDeviceClustering,
             "HistoryClustersOnDeviceClustering",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseEngagementScoreCache,
             "JourneysUseEngagementScoreCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceClusteringBlocklists,
             "JourneysOnDeviceClusteringBlocklist",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceClusteringKeywordFiltering,
             "JourneysKeywordFiltering",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceClusteringVisitRanking,
             "JourneysOnDeviceClusteringVisitRanking",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace history_clusters
