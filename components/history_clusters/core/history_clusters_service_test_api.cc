// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service_test_api.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

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
      visit.visit_row.visit_time =
          base::Time::Now() - base::TimeDelta::FromDays(2);
      visit.visit_row.visit_duration = base::TimeDelta::FromMilliseconds(5600);
      visit.context_annotations.page_end_reason = 3;
      visit.context_annotations.is_new_bookmark = true;
      visits.push_back(visit);
    }

    {
      history::AnnotatedVisit visit;
      visit.url_row.set_id(2);
      visit.url_row.set_url(GURL("https://github.com/"));
      visit.url_row.set_title(u"Code Storage Title");
      visit.visit_row.visit_id = 2;
      // Choose a recent time, as otherwise History will discard the visit.
      visit.visit_row.visit_time =
          base::Time::Now() - base::TimeDelta::FromDays(1);
      visit.visit_row.visit_duration = base::TimeDelta::FromSeconds(20);
      visit.visit_row.referring_visit = 1;
      visit.context_annotations.page_end_reason = 5;
      visit.context_annotations.is_existing_part_of_tab_group = true;
      visits.push_back(visit);
    }
  }

  return visits;
}

}  // namespace history_clusters
