// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_BACKEND_UTIL_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_BACKEND_UTIL_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"

namespace history {
class HistoryDatabase;

namespace journeys {

// Outcome of resolving the visits of one stored journey against the local
// `visits` and `urls` tables. Recorded once per stored journey by
// `GetAllJourneysWithResolvedVisits()`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncedJourneyResolutionResult)
enum class SyncedJourneyResolutionResult {
  // Every visit resolved to a URL row; the journey is returned.
  kResolved = 0,
  // A visit timestamp has no matching row in the `visits` table.
  kMissingVisit = 1,
  // A visit row exists, but its URL row is missing from the `urls` table.
  kMissingUrl = 2,
  kMaxValue = kMissingUrl,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/history/enums.xml:SyncedJourneyResolutionResult)

// Resolves the visit timestamps in `journey` to URLs and titles using the
// `visits` and `urls` tables in `db`. For redirect chains where multiple visits
// share the same visit timestamp, `GetLastRowForVisitByVisitTime` resolves to
// the end of the redirect chain (the most recently added visit row). If any
// visit entry cannot be found, returns `std::nullopt` (indicating the journey
// has missing visits and should be skipped). Returns a fully resolved `Journey`
// on success.
std::optional<Journey> ResolveJourneyVisits(HistoryDatabase& db,
                                            JourneyRow journey);

// Retrieves the journey identified by `journey_id`, with history entries
// resolved the same way as `GetAllJourneysWithResolvedVisits()`. Returns
// `std::nullopt` if there is no such journey, or if any of its visits cannot
// be resolved (matching the list, which excludes such journeys).
std::optional<Journey> GetJourneyWithResolvedVisits(
    HistoryDatabase& db,
    const std::string& journey_id);

// Retrieves all stored journeys, ordered by `creation_time` DESC, with
// history entries resolved to URLs and titles via the `visits` and `urls`
// tables. Journeys with unresolved visits are excluded. Records
// History.SyncedJourneys.Resolution.Result.
std::vector<Journey> GetAllJourneysWithResolvedVisits(HistoryDatabase& db);

// Returns the number of stored journeys in `db` that cannot be resolved on
// this device because at least one of their visit timestamps is missing in the
// local `visits` and `urls` tables. Inspects at most a fixed number of the most
// recently created journeys to bound the work on the History backend thread.
// Note: This helper is temporary for the Topics fishfood feedback export flow
// and will not stay in production code. It iterates over stored journeys and
// performs per-visit database lookups (`O(N_journeys * M_visits)`).
// TODO(crbug.com/568422896): Remove once Topics fishfood evaluation is
// complete.
size_t GetUnresolvableJourneysCountForFishfood(HistoryDatabase& db);

// Same as `GetUnresolvableJourneysCountForFishfood()`, but inspects at most
// `max_journeys` journeys. Exposed only so tests can exercise the bound.
size_t GetUnresolvableJourneysCountForFishfoodForTesting(HistoryDatabase& db,
                                                         size_t max_journeys);

}  // namespace journeys
}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_BACKEND_UTIL_H_
