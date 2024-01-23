// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/clustering_backend.h"
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
    std::vector<history::AnnotatedVisit> annotated_visits;

    base::CancelableTaskTracker tracker;
    history::QueryOptions options;
    options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
    history_service_->GetAnnotatedVisits(
        options,
        /*compute_redirect_chain_start_properties=*/true,
        /*get_unclustered_visits_only=*/false,
        base::BindLambdaForTesting(
            [&](std::vector<history::AnnotatedVisit> visits) {
              annotated_visits = std::move(visits);
            }),
        &tracker);
    history::BlockUntilHistoryProcessesPendingRequests(history_service_);

    return annotated_visits;
  }

  void SetClusteringBackendForTest(std::unique_ptr<ClusteringBackend> backend) {
    DCHECK(backend.get());
    history_clusters_service_->backend_ = std::move(backend);
  }

  void SetAllKeywordsCacheTimestamp(base::Time time) {
    history_clusters_service_->all_keywords_cache_timestamp_ = time;
  }

  void SetShortKeywordCacheTimestamp(base::Time time) {
    history_clusters_service_->short_keyword_cache_timestamp_ = time;
  }

  void SetShortKeywordCache(HistoryClustersService::KeywordMap cache) {
    history_clusters_service_->short_keyword_cache_ = cache;
  }

  void SetAllKeywordsCache(HistoryClustersService::KeywordMap cache) {
    history_clusters_service_->all_keywords_cache_ = cache;
  }

  HistoryClustersService* const history_clusters_service_;
  history::HistoryService* const history_service_;
};

// Fetches two hardcoded test visits.
std::vector<history::AnnotatedVisit> GetHardcodedTestVisits();

// Fetches the hardcoded `ClusterVisit` with ID `visit_id`.
history::ClusterVisit GetHardcodedClusterVisit(history::VisitID visit_id,
                                               float score = 0.5,
                                               int engagement_score = 0);

history::ClusterVisit AnnotatedVisitToClusterVisit(
    const history::AnnotatedVisit& visit,
    float score = .5,
    int engagement_score = 0);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TEST_API_H_
