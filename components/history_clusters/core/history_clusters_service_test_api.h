// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_

#include <vector>

#include "base/test/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/history_clusters_service.h"

namespace history_clusters {

class HistoryClustersServiceTestApi {
 public:
  explicit HistoryClustersServiceTestApi(
      HistoryClustersService* history_clusters_service,
      history::HistoryService* history_service)
      : history_clusters_service_(history_clusters_service),
        history_service_(history_service) {}

  // Gets the annotated visits from HistoryService synchronously for testing.
  std::vector<history::AnnotatedVisit> GetVisits() {
    std::vector<history::AnnotatedVisit> result;

    base::CancelableTaskTracker tracker;
    history_service_->GetAnnotatedVisits(
        1000,  // Getting 1000 clusters for testing is a reasonable fake value.
        base::BindLambdaForTesting(
            [&](std::vector<history::AnnotatedVisit> visits) {
              result = std::move(visits);
            }),
        &tracker);
    history::BlockUntilHistoryProcessesPendingRequests(history_service_);

    return result;
  }

  HistoryClustersService* const history_clusters_service_;
  history::HistoryService* const history_service_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_
