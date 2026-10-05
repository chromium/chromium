// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_backend_util.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history::journeys {

namespace {

// Upper bound on the number of journeys inspected by
// `GetUnresolvableJourneysCountForFishfood()`, to bound the per-visit database
// lookups performed on the History backend thread.
constexpr size_t kMaxJourneysForUnresolvableCount = 500;

size_t GetUnresolvableJourneysCount(HistoryDatabase& db, size_t max_journeys) {
  // Note: This is O(N_journeys * M_visits) because `ResolveJourneyVisits()`
  // performs per-visit database lookups. It is only invoked on-demand during
  // the temporary fishfood feedback export flow, and `max_journeys` bounds the
  // work on the History backend thread.
  std::vector<JourneyRow> journeys = db.GetAllJourneys();
  const size_t limit = std::min(journeys.size(), max_journeys);
  size_t unresolvable_count = 0;
  for (size_t i = 0; i < limit; ++i) {
    if (!ResolveJourneyVisits(db, std::move(journeys[i])).has_value()) {
      ++unresolvable_count;
    }
  }
  return unresolvable_count;
}

}  // namespace

std::optional<Journey> ResolveJourneyVisits(HistoryDatabase& db,
                                            JourneyRow journey) {
  std::vector<JourneyVisit> visits;
  visits.reserve(journey.history_entries.size());

  for (const JourneyHistoryEntry& entry : journey.history_entries) {
    VisitRow visit_row;
    if (!db.GetLastRowForVisitByVisitTime(entry.visit_time, &visit_row)) {
      VLOG(1) << "Skipping URL for journey_id=" << journey.journey_id
              << " with visit_time=" << entry.visit_time
              << ": not found in local visit database.";
      return std::nullopt;
    }

    URLRow url_row;
    if (!db.GetURLRow(visit_row.url_id, &url_row)) {
      VLOG(1) << "Skipping URL for journey_id=" << journey.journey_id
              << " with visit_time=" << entry.visit_time
              << ": URL not found for visit.";
      return std::nullopt;
    }

    visits.emplace_back(
        url_row.url(), url_row.title(), entry.visit_time,
        /*is_foreign=*/!visit_row.originator_cache_guid.empty());
  }

  return Journey(std::move(journey.journey_id), std::move(journey.title),
                 journey.creation_time, std::move(journey.emoji),
                 std::move(journey.overview), std::move(journey.short_overview),
                 std::move(visits), std::move(journey.continuation_queries));
}

std::optional<Journey> GetJourneyWithResolvedVisits(
    HistoryDatabase& db,
    const std::string& journey_id) {
  std::optional<JourneyRow> journey = db.GetJourney(journey_id);
  if (!journey.has_value()) {
    return std::nullopt;
  }
  return ResolveJourneyVisits(db, std::move(*journey));
}

std::vector<Journey> GetAllJourneysWithResolvedVisits(HistoryDatabase& db) {
  std::vector<JourneyRow> journeys = db.GetAllJourneys();
  std::vector<Journey> resolved_journeys;
  resolved_journeys.reserve(journeys.size());
  for (JourneyRow& journey : journeys) {
    std::optional<Journey> resolved =
        ResolveJourneyVisits(db, std::move(journey));
    if (resolved.has_value()) {
      resolved_journeys.push_back(std::move(*resolved));
    }
  }
  return resolved_journeys;
}

size_t GetUnresolvableJourneysCountForFishfood(HistoryDatabase& db) {
  return GetUnresolvableJourneysCount(db, kMaxJourneysForUnresolvableCount);
}

size_t GetUnresolvableJourneysCountForFishfoodForTesting(HistoryDatabase& db,
                                                         size_t max_journeys) {
  return GetUnresolvableJourneysCount(db, max_journeys);
}

}  // namespace history::journeys
