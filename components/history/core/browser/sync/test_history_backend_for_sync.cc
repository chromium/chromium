// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/test_history_backend_for_sync.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history {

TestHistoryBackendForSync::TestHistoryBackendForSync() = default;
TestHistoryBackendForSync::~TestHistoryBackendForSync() = default;

URLID TestHistoryBackendForSync::AddURL(URLRow row) {
  DCHECK_EQ(row.id(), 0);
  URLID id = next_url_id_++;
  row.set_id(id);
  urls_.push_back(std::move(row));
  return id;
}

VisitID TestHistoryBackendForSync::AddVisit(VisitRow row) {
  DCHECK_EQ(row.visit_id, 0);
  VisitID id = next_visit_id_++;
  row.visit_id = id;
  visits_.push_back(std::move(row));
  return id;
}

const std::vector<URLRow>& TestHistoryBackendForSync::GetURLs() const {
  return urls_;
}
const std::vector<VisitRow>& TestHistoryBackendForSync::GetVisits() const {
  return visits_;
}

bool TestHistoryBackendForSync::IsExpiredVisitTime(
    const base::Time& time) const {
  return time < base::Time::Now() - kExpiryThreshold;
}

VisitID TestHistoryBackendForSync::AddSyncedVisit(const GURL& url,
                                                  const std::u16string& title,
                                                  bool hidden,
                                                  const VisitRow& visit) {
  const URLRow& url_row = FindOrAddURL(url, title, hidden);

  VisitRow visit_to_add = visit;
  visit_to_add.url_id = url_row.id();
  AddVisit(std::move(visit_to_add));

  return visits_.back().visit_id;
}

bool TestHistoryBackendForSync::UpdateSyncedVisit(const VisitRow& visit) {
  for (VisitRow& existing_visit : visits_) {
    if (existing_visit.originator_cache_guid == visit.originator_cache_guid &&
        existing_visit.originator_visit_id == visit.originator_visit_id) {
      // `visit_id` and `url_id` aren't set in visits coming from Sync, so
      // keep those from the existing row.
      VisitRow new_visit = visit;
      new_visit.visit_id = existing_visit.visit_id;
      new_visit.url_id = existing_visit.url_id;
      existing_visit = new_visit;
      return true;
    }
  }
  return false;
}

const URLRow& TestHistoryBackendForSync::FindOrAddURL(
    const GURL& url,
    const std::u16string& title,
    bool hidden) {
  for (URLRow& candidate : urls_) {
    if (candidate.url() == url) {
      return candidate;
    }
  }
  // Not found; add a new URLRow.
  URLRow url_to_add(url);
  url_to_add.set_title(title);
  url_to_add.set_hidden(hidden);
  AddURL(std::move(url_to_add));
  return urls_.back();
}

}  // namespace history
