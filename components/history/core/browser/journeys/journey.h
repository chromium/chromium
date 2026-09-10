// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "url/gurl.h"

namespace history::journeys {

// Represents a visit entry within a journey that has been resolved to its URL
// and title in the history database.
struct JourneyVisit {
  JourneyVisit();
  JourneyVisit(GURL url, std::u16string title);
  ~JourneyVisit();
  JourneyVisit(const JourneyVisit&);
  JourneyVisit& operator=(const JourneyVisit&);
  JourneyVisit(JourneyVisit&&) noexcept;
  JourneyVisit& operator=(JourneyVisit&&) noexcept;

  bool operator==(const JourneyVisit& other) const = default;

  // Resolved URL of the visit from the history database.
  GURL url;

  // Resolved page title of the visit from the history database.
  std::u16string title;
};

// Represents a fully resolved journey with URLs and titles resolved from the
// history database, suitable for consumption by the UI and downstream features.
struct Journey {
  Journey();
  Journey(std::string journey_id,
          std::string title,
          base::Time creation_time,
          std::optional<std::string> emoji = std::nullopt,
          std::optional<std::string> overview = std::nullopt,
          std::optional<std::string> short_overview = std::nullopt,
          std::vector<JourneyVisit> visits = {},
          std::vector<JourneyContinuationQuery> continuation_queries = {});
  ~Journey();
  Journey(const Journey&);
  Journey& operator=(const Journey&);
  Journey(Journey&&) noexcept;
  Journey& operator=(Journey&&) noexcept;

  bool operator==(const Journey& other) const = default;

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

  // Resolved visit entries.
  std::vector<JourneyVisit> visits;

  // Associated continuation query suggestions.
  std::vector<JourneyContinuationQuery> continuation_queries;
};

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_H_
