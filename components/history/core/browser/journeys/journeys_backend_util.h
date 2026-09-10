// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_BACKEND_UTIL_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_BACKEND_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"

namespace history {
class HistoryDatabase;

namespace journeys {

// Resolves the visit timestamps in `journey` to URLs and titles using the
// `visits` and `urls` tables in `db`. For redirect chains where multiple visits
// share the same visit timestamp, `GetLastRowForVisitByVisitTime` resolves to
// the end of the redirect chain (the most recently added visit row). If any
// visit entry cannot be found, returns `std::nullopt` (indicating the journey
// has missing visits and should be skipped). Returns a fully resolved `Journey`
// on success.
std::optional<Journey> ResolveJourneyVisits(HistoryDatabase& db,
                                            JourneyRow journey);

// Retrieves all stored journeys, ordered by `creation_time` DESC, with
// history entries resolved to URLs and titles via the `visits` and `urls`
// tables. Journeys with unresolved visits are excluded.
std::vector<Journey> GetAllJourneysWithResolvedVisits(HistoryDatabase& db);

}  // namespace journeys
}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_BACKEND_UTIL_H_
