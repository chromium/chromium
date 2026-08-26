// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_ROW_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_ROW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace history::journeys {

// Represents a visit entry within a journey, linking the journey to a visit
// timestamp in the history database.
struct JourneyHistoryEntry {
  JourneyHistoryEntry();
  explicit JourneyHistoryEntry(base::Time visit_time);
  ~JourneyHistoryEntry();
  JourneyHistoryEntry(const JourneyHistoryEntry&);
  JourneyHistoryEntry& operator=(const JourneyHistoryEntry&);
  JourneyHistoryEntry(JourneyHistoryEntry&&) noexcept;
  JourneyHistoryEntry& operator=(JourneyHistoryEntry&&) noexcept;

  bool operator==(const JourneyHistoryEntry& other) const = default;

  // Timestamp of the visit in history.
  base::Time visit_time;
};

// Represents a continuation query suggestion associated with a journey.
struct JourneyContinuationQuery {
  JourneyContinuationQuery();
  JourneyContinuationQuery(std::string title, std::string prompt);
  ~JourneyContinuationQuery();
  JourneyContinuationQuery(const JourneyContinuationQuery&);
  JourneyContinuationQuery& operator=(const JourneyContinuationQuery&);
  JourneyContinuationQuery(JourneyContinuationQuery&&) noexcept;
  JourneyContinuationQuery& operator=(JourneyContinuationQuery&&) noexcept;

  bool operator==(const JourneyContinuationQuery& other) const = default;

  std::string title;
  std::string prompt;
};

// Holds all information associated with one journey record in the history
// database.
struct JourneyRow {
  JourneyRow();
  JourneyRow(std::string journey_id,
             std::string title,
             base::Time creation_time,
             std::optional<std::string> emoji = std::nullopt,
             std::optional<std::string> overview = std::nullopt,
             std::optional<std::string> short_overview = std::nullopt,
             std::vector<JourneyHistoryEntry> history_entries = {},
             std::vector<JourneyContinuationQuery> continuation_queries = {});
  ~JourneyRow();
  JourneyRow(const JourneyRow&);
  JourneyRow& operator=(const JourneyRow&);
  JourneyRow(JourneyRow&&) noexcept;
  JourneyRow& operator=(JourneyRow&&) noexcept;

  bool operator==(const JourneyRow& other) const = default;

  // Unique identifier for the journey (GUID).
  std::string journey_id;

  // Display title of the journey.
  std::string title;

  // Creation timestamp of the journey.
  base::Time creation_time;

  // Optional emoji representing the journey.
  std::optional<std::string> emoji;

  // Optional long overview summary.
  std::optional<std::string> overview;

  // Optional short overview summary.
  std::optional<std::string> short_overview;

  // Associated visit entries.
  std::vector<JourneyHistoryEntry> history_entries;

  // Associated continuation query suggestions.
  std::vector<JourneyContinuationQuery> continuation_queries;
};

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_ROW_H_
