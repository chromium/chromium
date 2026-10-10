// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_backend_util.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
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

// The journey's resolved visits by visit time. `base::Time` can't be a hash map
// key (it has no Abseil hash), so the key is its microsecond value, which is
// the precision the database stores.
using VisitsByTime = absl::flat_hash_map<int64_t, const JourneyVisit*>;

// Returns the visits that the items of `collection` refer to, in display order.
// Items that don't match one of the journey's visits are skipped.
std::vector<JourneyVisit> ResolveCollectionItems(
    const JourneyHistoryEntryCollection& collection,
    const VisitsByTime& visits_by_time,
    std::string_view journey_id) {
  std::vector<JourneyVisit> collection_visits;
  collection_visits.reserve(collection.items.size());
  for (const JourneyHistoryEntry& item : collection.items) {
    auto it = visits_by_time.find(
        item.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    if (it == visits_by_time.end()) {
      // TODO(crbug.com/568227865): Consider recording dropped collection items
      // in a histogram.
      VLOG(1) << "Skipping collection item for journey_id=" << journey_id
              << " with visit_time=" << item.visit_time
              << ": not one of the journey's visits.";
      continue;
    }
    collection_visits.push_back(*it->second);
  }
  return collection_visits;
}

// Resolves the items of `journey.collections` against the journey's already
// resolved `visits`. Collections are optional, so they never cause the journey
// to fail resolution: items that don't match one of the journey's visits are
// dropped, and so are collections left without any items.
std::vector<JourneyVisitCollection> ResolveCollections(
    const JourneyRow& journey,
    const std::vector<JourneyVisit>& visits) {
  if (journey.collections.empty()) {
    return {};
  }

  VisitsByTime visits_by_time;
  visits_by_time.reserve(visits.size());
  for (const JourneyVisit& visit : visits) {
    visits_by_time.emplace(
        visit.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds(), &visit);
  }

  std::vector<JourneyVisitCollection> collections;
  for (const JourneyHistoryEntryCollection& collection : journey.collections) {
    std::vector<JourneyVisit> collection_visits =
        ResolveCollectionItems(collection, visits_by_time, journey.journey_id);
    if (!collection_visits.empty()) {
      collections.emplace_back(collection.title, std::move(collection_visits));
    }
  }
  return collections;
}

// Same as `ResolveJourneyVisits()`, but on failure reports which lookup
// failed. The error is never `SyncedJourneyResolutionResult::kResolved`.
base::expected<Journey, SyncedJourneyResolutionResult>
ResolveJourneyVisitsWithResult(HistoryDatabase& db, JourneyRow journey) {
  std::vector<JourneyVisit> visits;
  visits.reserve(journey.history_entries.size());

  for (const JourneyHistoryEntry& entry : journey.history_entries) {
    VisitRow visit_row;
    if (!db.GetLastRowForVisitByVisitTime(entry.visit_time, &visit_row)) {
      VLOG(1) << "Skipping URL for journey_id=" << journey.journey_id
              << " with visit_time=" << entry.visit_time
              << ": not found in local visit database.";
      return base::unexpected(SyncedJourneyResolutionResult::kMissingVisit);
    }

    URLRow url_row;
    if (!db.GetURLRow(visit_row.url_id, &url_row)) {
      VLOG(1) << "Skipping URL for journey_id=" << journey.journey_id
              << " with visit_time=" << entry.visit_time
              << ": URL not found for visit.";
      return base::unexpected(SyncedJourneyResolutionResult::kMissingUrl);
    }

    visits.emplace_back(
        url_row.url(), url_row.title(), entry.visit_time,
        /*is_foreign=*/!visit_row.originator_cache_guid.empty());
  }

  std::vector<JourneyVisitCollection> collections =
      ResolveCollections(journey, visits);

  return Journey(std::move(journey.journey_id), std::move(journey.title),
                 journey.creation_time, std::move(journey.emoji),
                 std::move(journey.overview), std::move(journey.short_overview),
                 std::move(visits), std::move(journey.continuation_queries),
                 std::move(collections));
}

}  // namespace

std::optional<Journey> ResolveJourneyVisits(HistoryDatabase& db,
                                            JourneyRow journey) {
  return base::OptionalFromExpected(
      ResolveJourneyVisitsWithResult(db, std::move(journey)));
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
    base::expected<Journey, SyncedJourneyResolutionResult> resolved =
        ResolveJourneyVisitsWithResult(db, std::move(journey));
    base::UmaHistogramEnumeration("History.SyncedJourneys.Resolution.Result",
                                  resolved.has_value()
                                      ? SyncedJourneyResolutionResult::kResolved
                                      : resolved.error());
    if (resolved.has_value()) {
      resolved_journeys.push_back(std::move(resolved).value());
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
