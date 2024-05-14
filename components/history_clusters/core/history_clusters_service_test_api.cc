// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service_test_api.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters {

// static
std::vector<history::AnnotatedVisit> GetHardcodedTestVisits() {
  // For non-flakiness, return a static list of visits, as this relies on Now().
  static std::vector<history::AnnotatedVisit> visits;

  if (visits.empty()) {
    {
      history::AnnotatedVisit visit;
      visit.url_row.set_id(1);
      visit.url_row.set_url(GURL("https://google.com/"));
      visit.url_row.set_title(u"Search Engine Title");
      visit.visit_row.visit_id = 1;
      // Choose a recent time, as otherwise History will discard the visit.
      visit.visit_row.visit_time = base::Time::Now() - base::Days(2);
      visit.visit_row.visit_duration = base::Milliseconds(5600);
      visit.context_annotations.page_end_reason = 3;
      visit.context_annotations.is_new_bookmark = true;
      visit.source = history::VisitSource::SOURCE_BROWSED;
      visits.push_back(visit);
    }

    {
      history::AnnotatedVisit visit;
      visit.url_row.set_id(2);
      visit.url_row.set_url(GURL("https://github.com/"));
      visit.url_row.set_title(u"Code Storage Title");
      visit.visit_row.visit_id = 2;
      // Choose a recent time, as otherwise History will discard the visit.
      visit.visit_row.visit_time = base::Time::Now() - base::Days(1);
      visit.visit_row.visit_duration = base::Seconds(20);
      visit.visit_row.referring_visit = 1;
      visit.context_annotations.page_end_reason = 5;
      visit.context_annotations.is_existing_part_of_tab_group = true;
      visit.source = history::VisitSource::SOURCE_BROWSED;
      visits.push_back(visit);
    }

    {
      // Synced visits should not be included when fetching visits to cluster.
      history::AnnotatedVisit visit;
      visit.url_row.set_id(3);
      visit.url_row.set_url(GURL("https://synched-visit.com/"));
      visit.url_row.set_title(u"Synched visit");
      visit.visit_row.visit_id = 3;
      // Choose a recent time, as otherwise History will discard the visit.
      visit.visit_row.visit_time = base::Time::Now() - base::Days(1);
      visit.visit_row.visit_duration = base::Seconds(20);
      visit.context_annotations.page_end_reason = 5;
      visit.source = history::VisitSource::SOURCE_SYNCED;
      visits.push_back(visit);
    }

    {
      // Visits older than 30 days should not be included in keyword requests.
      history::AnnotatedVisit visit;
      visit.url_row.set_id(4);
      visit.url_row.set_url(GURL("https://31-day-old-visit.com/"));
      visit.url_row.set_title(u"31 day old visit");
      visit.visit_row.visit_id = 4;
      // Choose a recent time, as otherwise History will discard the visit.
      visit.visit_row.visit_time = base::Time::Now() - base::Days(60);
      visit.visit_row.visit_duration = base::Seconds(20);
      visit.visit_row.referring_visit = 1;
      visit.context_annotations.page_end_reason = 5;
      visit.context_annotations.is_existing_part_of_tab_group = true;
      visit.source = history::VisitSource::SOURCE_BROWSED;
      visits.push_back(visit);
    }

    {
      // A second 1-day-old visit.
      history::AnnotatedVisit visit;
      visit.url_row.set_id(5);
      visit.url_row.set_url(GURL("https://second-1-day-old-visit.com/"));
      visit.url_row.set_title(u"second-1-day-old-visit");
      visit.visit_row.visit_id = 5;
      // Choose a recent time, as otherwise History will discard the visit.
      visit.visit_row.visit_time = base::Time::Now() - base::Days(1);
      visit.visit_row.visit_duration = base::Seconds(20);
      visit.visit_row.referring_visit = 1;
      visit.context_annotations.page_end_reason = 5;
      visit.context_annotations.is_new_bookmark = true;
      visit.source = history::VisitSource::SOURCE_BROWSED;
      visits.push_back(visit);
    }
  }

  return visits;
}

history::ClusterVisit GetHardcodedClusterVisit(history::VisitID visit_id,
                                               float score,
                                               int engagement_score) {
  for (const auto& visit : GetHardcodedTestVisits()) {
    if (visit.visit_row.visit_id == visit_id)
      return AnnotatedVisitToClusterVisit(visit, score, engagement_score);
  }

  NOTREACHED_IN_MIGRATION()
      << "GetHardcodedClusterVisit() could not find visit_id: " << visit_id;
  return {};
}

history::ClusterVisit AnnotatedVisitToClusterVisit(
    const history::AnnotatedVisit& visit,
    float score,
    int engagement_score) {
  history::ClusterVisit cluster_visit;
  cluster_visit.annotated_visit = visit;
  cluster_visit.normalized_url = visit.url_row.url();
  cluster_visit.url_for_deduping =
      ComputeURLForDeduping(cluster_visit.normalized_url);
  cluster_visit.score = score;
  cluster_visit.engagement_score = engagement_score;
  return cluster_visit;
}

}  // namespace history_clusters
