// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_

#include <vector>

#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

class HistoryClustersServiceTestApi {
 public:
  explicit HistoryClustersServiceTestApi(
      HistoryClustersService* history_clusters_service)
      : history_clusters_service_(history_clusters_service) {}

  std::vector<history::AnnotatedVisit> GetVisits() const {
    return history_clusters_service_->visits_;
  }

  HistoryClustersService* history_clusters_service_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_
