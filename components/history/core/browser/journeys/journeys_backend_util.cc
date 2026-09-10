// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_backend_util.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_database.h"
#include "url/gurl.h"

namespace history::journeys {

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

    visits.emplace_back(url_row.url(), url_row.title());
  }

  return Journey(std::move(journey.journey_id), std::move(journey.title),
                 journey.creation_time, std::move(journey.emoji),
                 std::move(journey.overview), std::move(journey.short_overview),
                 std::move(visits), std::move(journey.continuation_queries));
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

}  // namespace history::journeys
