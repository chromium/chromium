// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_NTP_VISIT_SCORES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_NTP_VISIT_SCORES_H_

#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// Returns the score for the visit attributes for the NTP History Clusters
// module.
float GetNtpVisitAttributesScore(const history::ClusterVisit& visit);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_NTP_VISIT_SCORES_H_
