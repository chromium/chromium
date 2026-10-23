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

#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "components/history/core/browser/journeys/journeys_database.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"

namespace history::journeys {

namespace {

using JourneyHistoryEntryLookupSpan =
    base::span<const JourneyHistoryEntryLookup>;
using LookupsByJourneyId =
    absl::flat_hash_map<std::string, std::vector<JourneyHistoryEntryLookup>>;

// Groups `lookups` by journey ID, keeping their relative order, which is visit
// time order as returned by
// `JourneysDatabase::LookUpAllJourneyHistoryEntries()`.
LookupsByJourneyId GroupByJourneyId(
    std::vector<JourneyHistoryEntryLookup> lookups) {
  LookupsByJourneyId grouped;
  for (JourneyHistoryEntryLookup& lookup : lookups) {
    grouped[lookup.journey_id].push_back(std::move(lookup));
  }
  return grouped;
}

// Returns the lookups for `journey_id` in `grouped`, or an empty span if the
// journey has no history entries.
JourneyHistoryEntryLookupSpan FindLookups(const LookupsByJourneyId& grouped,
                                          const std::string& journey_id) {
  const std::vector<JourneyHistoryEntryLookup>* found =
      base::FindOrNull(grouped, journey_id);
  return found ? JourneyHistoryEntryLookupSpan(*found)
               : JourneyHistoryEntryLookupSpan();
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

// Builds the resolved `Journey` for `journey` from the `lookups` of its history
// entries. `journey.history_entries` is not used: `lookups` holds the same
// entries, already looked up in `visits` and `urls`. On failure reports which
// lookup failed. The error is never `SyncedJourneyResolutionResult::kResolved`.
base::expected<Journey, SyncedJourneyResolutionResult> ResolveJourney(
    JourneyRow journey,
    JourneyHistoryEntryLookupSpan lookups) {
  if (lookups.empty()) {
    VLOG(1) << "Skipping journey_id=" << journey.journey_id
            << ": no history entries.";
    return base::unexpected(SyncedJourneyResolutionResult::kMissingVisit);
  }

  std::vector<JourneyVisit> visits;
  visits.reserve(lookups.size());

  for (const JourneyHistoryEntryLookup& lookup : lookups) {
    switch (lookup.status) {
      case JourneyHistoryEntryLookup::Status::kFound:
        visits.emplace_back(lookup.url, lookup.title, lookup.visit_time,
                            lookup.is_foreign);
        break;
      case JourneyHistoryEntryLookup::Status::kMissingVisit:
        VLOG(1) << "Skipping URL for journey_id=" << journey.journey_id
                << " with visit_time=" << lookup.visit_time
                << ": not found in local visit database.";
        return base::unexpected(SyncedJourneyResolutionResult::kMissingVisit);
      case JourneyHistoryEntryLookup::Status::kMissingUrl:
        VLOG(1) << "Skipping URL for journey_id=" << journey.journey_id
                << " with visit_time=" << lookup.visit_time
                << ": URL not found for visit.";
        return base::unexpected(SyncedJourneyResolutionResult::kMissingUrl);
    }
  }

  std::vector<JourneyVisitCollection> collections =
      ResolveCollections(journey, visits);

  return Journey(std::move(journey.journey_id), std::move(journey.title),
                 journey.creation_time, std::move(journey.emoji),
                 std::move(journey.overview), std::move(journey.short_overview),
                 std::move(visits), std::move(journey.continuation_queries),
                 std::move(collections));
}

// Resolves all stored journeys, using one lookup query for all of them.
// Returns the results ordered by `creation_time` DESC, or none if the lookup
// query fails, the same all-or-nothing rule as
// `JourneysDatabase::GetAllJourneys()`.
std::vector<base::expected<Journey, SyncedJourneyResolutionResult>>
ResolveStoredJourneys(HistoryDatabase& db) {
  std::vector<JourneyRow> journeys = db.GetAllJourneys();
  if (journeys.empty()) {
    return {};
  }
  std::optional<std::vector<JourneyHistoryEntryLookup>> lookups =
      db.LookUpAllJourneyHistoryEntries();
  if (!lookups.has_value()) {
    return {};
  }
  const LookupsByJourneyId grouped = GroupByJourneyId(std::move(*lookups));

  std::vector<base::expected<Journey, SyncedJourneyResolutionResult>> results;
  results.reserve(journeys.size());
  for (JourneyRow& journey : journeys) {
    const JourneyHistoryEntryLookupSpan journey_lookups =
        FindLookups(grouped, journey.journey_id);
    results.push_back(ResolveJourney(std::move(journey), journey_lookups));
  }
  return results;
}

}  // namespace

std::optional<Journey> GetJourneyWithResolvedVisits(
    HistoryDatabase& db,
    const std::string& journey_id) {
  std::optional<JourneyRow> journey = db.GetJourney(journey_id);
  if (!journey.has_value()) {
    return std::nullopt;
  }
  const std::optional<std::vector<JourneyHistoryEntryLookup>> lookups =
      db.LookUpJourneyHistoryEntries(journey_id);
  if (!lookups.has_value()) {
    return std::nullopt;
  }
  return base::OptionalFromExpected(
      ResolveJourney(std::move(*journey), *lookups));
}

std::vector<Journey> GetAllJourneysWithResolvedVisits(HistoryDatabase& db) {
  std::vector<Journey> resolved_journeys;
  for (base::expected<Journey, SyncedJourneyResolutionResult>& resolved :
       ResolveStoredJourneys(db)) {
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
  const std::vector<base::expected<Journey, SyncedJourneyResolutionResult>>
      results = ResolveStoredJourneys(db);
  return static_cast<size_t>(std::ranges::count_if(
      results, [](const auto& result) { return !result.has_value(); }));
}

}  // namespace history::journeys
