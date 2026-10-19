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

// Retrieves the journey identified by `journey_id`, with history entries
// resolved the same way as `GetAllJourneysWithResolvedVisits()`. Returns
// `std::nullopt` if there is no such journey, if any of its visits cannot be
// resolved (matching the list, which excludes such journeys), or on a database
// error.
std::optional<Journey> GetJourneyWithResolvedVisits(
    HistoryDatabase& db,
    const std::string& journey_id);

// Retrieves all stored journeys, ordered by `creation_time` DESC, with history
// entries resolved to URLs and titles via one
// `JourneysDatabase::LookUpAllJourneyHistoryEntries()` query; see there for
// redirect-chain handling. A journey with any unresolvable entry is dropped
// whole. Returns no journeys on a database error. Records
// History.SyncedJourneys.Resolution.Result.
std::vector<Journey> GetAllJourneysWithResolvedVisits(HistoryDatabase& db);

// Returns the number of stored journeys in `db` that cannot be resolved on
// this device because at least one of their visit timestamps is missing in the
// local `visits` and `urls` tables.
// Note: This helper is temporary for the Topics fishfood feedback export flow
// and will not stay in production code.
// TODO(crbug.com/568422896): Remove once Topics fishfood evaluation is
// complete.
size_t GetUnresolvableJourneysCountForFishfood(HistoryDatabase& db);

}  // namespace journeys
}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_BACKEND_UTIL_H_
