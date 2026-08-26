// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journey_row.h"

#include <utility>

namespace history::journeys {

JourneyHistoryEntry::JourneyHistoryEntry() = default;

JourneyHistoryEntry::JourneyHistoryEntry(base::Time visit_time)
    : visit_time(visit_time) {}

JourneyHistoryEntry::~JourneyHistoryEntry() = default;
JourneyHistoryEntry::JourneyHistoryEntry(const JourneyHistoryEntry&) = default;
JourneyHistoryEntry& JourneyHistoryEntry::operator=(
    const JourneyHistoryEntry&) = default;
JourneyHistoryEntry::JourneyHistoryEntry(JourneyHistoryEntry&&) noexcept = default;
JourneyHistoryEntry& JourneyHistoryEntry::operator=(
    JourneyHistoryEntry&&) noexcept = default;

JourneyContinuationQuery::JourneyContinuationQuery() = default;

JourneyContinuationQuery::JourneyContinuationQuery(std::string title,
                                                   std::string prompt)
    : title(std::move(title)), prompt(std::move(prompt)) {}

JourneyContinuationQuery::~JourneyContinuationQuery() = default;
JourneyContinuationQuery::JourneyContinuationQuery(
    const JourneyContinuationQuery&) = default;
JourneyContinuationQuery& JourneyContinuationQuery::operator=(
    const JourneyContinuationQuery&) = default;
JourneyContinuationQuery::JourneyContinuationQuery(
    JourneyContinuationQuery&&) noexcept = default;
JourneyContinuationQuery& JourneyContinuationQuery::operator=(
    JourneyContinuationQuery&&) noexcept = default;

JourneyRow::JourneyRow() = default;

JourneyRow::JourneyRow(
    std::string journey_id,
    std::string title,
    base::Time creation_time,
    std::optional<std::string> emoji,
    std::optional<std::string> overview,
    std::optional<std::string> short_overview,
    std::vector<JourneyHistoryEntry> history_entries,
    std::vector<JourneyContinuationQuery> continuation_queries)
    : journey_id(std::move(journey_id)),
      title(std::move(title)),
      creation_time(creation_time),
      emoji(std::move(emoji)),
      overview(std::move(overview)),
      short_overview(std::move(short_overview)),
      history_entries(std::move(history_entries)),
      continuation_queries(std::move(continuation_queries)) {}

JourneyRow::~JourneyRow() = default;
JourneyRow::JourneyRow(const JourneyRow&) = default;
JourneyRow& JourneyRow::operator=(const JourneyRow&) = default;
JourneyRow::JourneyRow(JourneyRow&&) noexcept = default;
JourneyRow& JourneyRow::operator=(JourneyRow&&) noexcept = default;

}  // namespace history::journeys
