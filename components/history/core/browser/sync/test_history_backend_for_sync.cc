// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/test_history_backend_for_sync.h"

#include <vector>

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

bool TestHistoryBackendForSync::UpdateURL(URLRow row) {
  DCHECK_NE(row.id(), 0);
  for (URLRow& url : urls_) {
    if (url.id() == row.id()) {
      url = row;
      return true;
    }
  }
  return false;
}

VisitID TestHistoryBackendForSync::AddVisit(VisitRow row) {
  DCHECK_EQ(row.visit_id, 0);
  VisitID id = next_visit_id_++;
  row.visit_id = id;
  visits_.push_back(std::move(row));
  return id;
}

bool TestHistoryBackendForSync::UpdateVisit(VisitRow row) {
  DCHECK_NE(row.visit_id, 0);
  for (VisitRow& visit : visits_) {
    if (visit.visit_id == row.visit_id) {
      visit = row;
      return true;
    }
  }
  return false;
}

void TestHistoryBackendForSync::AddOrReplaceContentAnnotation(
    VisitID visit_id,
    const VisitContentAnnotations& content_annotation) {
  DCHECK_NE(visit_id, 0);
  content_annotations_[visit_id] = content_annotation;
}

void TestHistoryBackendForSync::RemoveURLAndVisits(URLID url_id) {
  std::erase_if(visits_, [url_id](const VisitRow& visit) {
    return visit.url_id == url_id;
  });
  std::erase_if(urls_,
                [url_id](const URLRow& url) { return url.id() == url_id; });
}

void TestHistoryBackendForSync::Clear() {
  urls_.clear();
  visits_.clear();
}

const std::vector<URLRow>& TestHistoryBackendForSync::GetURLs() const {
  return urls_;
}

const std::vector<VisitRow>& TestHistoryBackendForSync::GetVisits() const {
  return visits_;
}

const URLRow* TestHistoryBackendForSync::FindURLRow(const GURL& url) const {
  for (const URLRow& url_row : urls_) {
    if (url_row.url() == url) {
      return &url_row;
    }
  }
  return nullptr;
}

bool TestHistoryBackendForSync::CanAddURL(const GURL& url) const {
  return true;
}

bool TestHistoryBackendForSync::IsExpiredVisitTime(
    const base::Time& time) const {
  return time < base::Time::Now() - kExpiryThreshold;
}

bool TestHistoryBackendForSync::GetURLByID(URLID url_id, URLRow* url_row) {
  for (const URLRow& row : urls_) {
    if (row.id() == url_id) {
      *url_row = row;
      return true;
    }
  }
  return false;
}

bool TestHistoryBackendForSync::GetVisitByID(VisitID visit_id,
                                             VisitRow* visit_row) {
  for (const VisitRow& row : visits_) {
    if (row.visit_id == visit_id) {
      *visit_row = row;
      return true;
    }
  }
  return false;
}

bool TestHistoryBackendForSync::GetMostRecentVisitForURL(URLID id,
                                                         VisitRow* visit_row) {
  *visit_row = VisitRow();
  for (const VisitRow& candidate : visits_) {
    if (candidate.url_id == id &&
        (candidate.visit_time > visit_row->visit_time ||
         (candidate.visit_time == visit_row->visit_time &&
          candidate.visit_id > visit_row->visit_id))) {
      *visit_row = candidate;
    }
  }
  return visit_row->visit_id != 0;
}

bool TestHistoryBackendForSync::GetLastVisitByTime(base::Time visit_time,
                                                   VisitRow* visit_row) {
  *visit_row = VisitRow();
  // If there are multiple matches for `visit_time`, pick the one with the
  // largest ID.
  for (const VisitRow& candidate : visits_) {
    if (candidate.visit_time == visit_time &&
        candidate.visit_id > visit_row->visit_id) {
      *visit_row = candidate;
    }
  }
  return visit_row->visit_id != 0;
}

VisitVector TestHistoryBackendForSync::GetRedirectChain(VisitRow visit) {
  VisitVector result;
  result.push_back(visit);
  while (!(visit.transition & ui::PAGE_TRANSITION_CHAIN_START)) {
    if (!FindVisit(visit.referring_visit, &visit)) {
      ADD_FAILURE() << "Data error: referring_visit " << visit.referring_visit
                    << " not found";
      return {};
    }
    result.push_back(visit);
  }
  std::reverse(result.begin(), result.end());
  return result;
}

bool TestHistoryBackendForSync::GetForeignVisit(
    const std::string& originator_cache_guid,
    VisitID originator_visit_id,
    VisitRow* visit_row) {
  ++get_foreign_visit_call_count_;

  for (const VisitRow& candidate : visits_) {
    if (candidate.originator_cache_guid == originator_cache_guid &&
        candidate.originator_visit_id == originator_visit_id) {
      *visit_row = candidate;
      return true;
    }
  }
  return false;
}

std::vector<AnnotatedVisit>
TestHistoryBackendForSync::ToAnnotatedVisitsFromRows(
    const VisitVector& visit_rows,
    bool compute_redirect_chain_start_properties) {
  std::vector<AnnotatedVisit> annotated_visits;
  for (const VisitRow& visit_row : visit_rows) {
    URLRow url_row;
    GetURLByID(visit_row.url_id, &url_row);
    VisitContextAnnotations context_annotations;
    if (context_annotations_.count(visit_row.visit_id)) {
      context_annotations = context_annotations_[visit_row.visit_id];
    }
    VisitContentAnnotations content_annotations;
    if (content_annotations_.count(visit_row.visit_id)) {
      content_annotations = content_annotations_[visit_row.visit_id];
    }
    annotated_visits.emplace_back(url_row, visit_row, context_annotations,
                                  content_annotations, 0, 0, SOURCE_SYNCED);
  }
  return annotated_visits;
}

VisitID TestHistoryBackendForSync::AddSyncedVisit(
    const GURL& url,
    const std::u16string& title,
    bool hidden,
    const VisitRow& visit,
    const std::optional<VisitContextAnnotations>& context_annotations,
    const std::optional<VisitContentAnnotations>& content_annotations) {
  const URLRow& url_row = FindOrAddURL(url, title, hidden);

  VisitRow visit_to_add = visit;
  visit_to_add.url_id = url_row.id();
  VisitID visit_id = AddVisit(std::move(visit_to_add));

  if (context_annotations) {
    context_annotations_[visit_id] = *context_annotations;
  }
  if (content_annotations) {
    content_annotations_[visit_id] = *content_annotations;
  }

  return visit_id;
}

VisitID TestHistoryBackendForSync::UpdateSyncedVisit(
    const GURL& url,
    const std::u16string& title,
    bool hidden,
    const VisitRow& visit,
    const std::optional<VisitContextAnnotations>& context_annotations,
    const std::optional<VisitContentAnnotations>& content_annotations) {
  for (URLRow& existing_url : urls_) {
    if (existing_url.url() == url) {
      existing_url.set_title(title);
      existing_url.set_hidden(hidden);
    }
  }

  for (VisitRow& existing_visit : visits_) {
    if (existing_visit.originator_cache_guid == visit.originator_cache_guid &&
        existing_visit.originator_visit_id == visit.originator_visit_id) {
      VisitID visit_id = existing_visit.visit_id;

      VisitRow new_visit = visit;
      // `visit_id` and `url_id` aren't set in visits coming from Sync, so
      // keep those from the existing row.
      new_visit.visit_id = visit_id;
      new_visit.url_id = existing_visit.url_id;
      // Similarly, any `referring_visit` and `opener_visit` should be retained.
      // Note that these are the *local* versions of these IDs, not the
      // originator ones.
      new_visit.referring_visit = existing_visit.referring_visit;
      new_visit.opener_visit = existing_visit.opener_visit;

      existing_visit = new_visit;

      // If context / content annotations are provided, update those too. Note
      // that only parts of these are synced.
      if (context_annotations) {
        if (context_annotations_.count(visit_id)) {
          // For now, only the on-visit are synced, so only overwrite those.
          context_annotations_[visit_id].on_visit =
              context_annotations->on_visit;
        } else {
          context_annotations_[visit_id] = *context_annotations;
        }
      }
      if (content_annotations) {
        // Only set the fields that are actually synced.
        content_annotations_[visit_id].page_language =
            content_annotations->page_language;
        content_annotations_[visit_id].password_state =
            content_annotations->password_state;
      }

      return existing_visit.visit_id;
    }
  }
  return 0;
}

bool TestHistoryBackendForSync::UpdateVisitReferrerOpenerIDs(
    VisitID visit_id,
    VisitID referrer_id,
    VisitID opener_id) {
  for (VisitRow& visit : visits_) {
    if (visit.visit_id == visit_id) {
      visit.referring_visit = referrer_id;
      visit.opener_visit = opener_id;
      return true;
    }
  }
  return false;
}

void TestHistoryBackendForSync::AddVisitToSyncedCluster(
    const ClusterVisit& cluster_visit,
    const std::string& originator_cache_guid,
    int64_t cluster_id) {
  ++add_visit_to_synced_cluster_count_;
}

int64_t TestHistoryBackendForSync::GetClusterIdContainingVisit(
    VisitID visit_id) {
  // For testing purposes, just put every visit in a different cluster.
  return 1000 + static_cast<int64_t>(visit_id);
}

std::vector<GURL> TestHistoryBackendForSync::GetFaviconURLsForURL(
    const GURL& page_url) {
  // For the unit tests based on this class, favicon URLs aren't required.
  return {};
}

void TestHistoryBackendForSync::MarkVisitAsKnownToSync(VisitID visit_id) {
  for (auto& visit : visits_) {
    if (visit.visit_id == visit_id) {
      // This persists into the vector because we're operating on a reference.
      visit.is_known_to_sync = true;
    }
  }
}

void TestHistoryBackendForSync::DeleteAllForeignVisitsAndResetIsKnownToSync() {
  ++delete_all_foreign_visits_call_count_;

  for (auto it = visits_.begin(); it != visits_.end();) {
    VisitRow& visit = *it;
    visit.is_known_to_sync = false;

    if (visit.originator_cache_guid.empty()) {
      // Local visit, leave it.
      ++it;
      continue;
    } else {
      // Foreign visit, erase it along with any annotations.
      context_annotations_.erase(visit.visit_id);
      content_annotations_.erase(visit.visit_id);
      it = visits_.erase(it);
      // Note: The real backend would also erase the corresponding URL, if this
      // was the last remaining visit to it. That could be implemented here too,
      // but currently isn't necessary for the unit tests that use this class.
    }
  }
}

void TestHistoryBackendForSync::AddObserver(HistoryBackendObserver* observer) {
  observers_.AddObserver(observer);
}

void TestHistoryBackendForSync::RemoveObserver(
    HistoryBackendObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestHistoryBackendForSync::FindVisit(VisitID id, VisitRow* result) {
  for (const VisitRow& candidate : visits_) {
    if (candidate.visit_id == id) {
      *result = candidate;
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
