// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journey.h"

#include <utility>

namespace history::journeys {

JourneyVisit::JourneyVisit() = default;

JourneyVisit::JourneyVisit(GURL url, std::u16string title)
    : url(std::move(url)), title(std::move(title)) {}

JourneyVisit::~JourneyVisit() = default;
JourneyVisit::JourneyVisit(const JourneyVisit&) = default;
JourneyVisit& JourneyVisit::operator=(const JourneyVisit&) = default;
JourneyVisit::JourneyVisit(JourneyVisit&&) noexcept = default;
JourneyVisit& JourneyVisit::operator=(JourneyVisit&&) noexcept = default;

Journey::Journey() = default;

Journey::Journey(std::string journey_id,
                 std::string title,
                 base::Time creation_time,
                 std::optional<std::string> emoji,
                 std::optional<std::string> overview,
                 std::optional<std::string> short_overview,
                 std::vector<JourneyVisit> visits,
                 std::vector<JourneyContinuationQuery> continuation_queries)
    : journey_id(std::move(journey_id)),
      title(std::move(title)),
      creation_time(creation_time),
      emoji(std::move(emoji)),
      overview(std::move(overview)),
      short_overview(std::move(short_overview)),
      visits(std::move(visits)),
      continuation_queries(std::move(continuation_queries)) {}

Journey::~Journey() = default;
Journey::Journey(const Journey&) = default;
Journey& Journey::operator=(const Journey&) = default;
Journey::Journey(Journey&&) noexcept = default;
Journey& Journey::operator=(Journey&&) noexcept = default;

}  // namespace history::journeys
