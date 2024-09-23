// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_SCHEMA_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_SCHEMA_H_

#include <array>
#include <map>
#include <string>

namespace visited_url_ranking {

enum URLVisitAggregateRankingModelInputSignals {
  kTimeSinceLastModifiedSec = 0,
  kTimeSinceLastActiveSec = 1,
  kTimeActiveForTimePeriodSec = 2,
  kNumTimesActive = 3,
  kLocalTabCount = 4,
  kSessionTabCount = 5,
  kVisitCount = 6,
  kIsBookmarked = 7,
  kIsPinned = 8,
  kIsInTabGroup = 9,
  kIsInCluster = 10,
  kHasUrlKeyedImage = 11,
  kHasAppId = 12,
  kPlatform = 13,
  kSeenCountLastDay = 14,
  kActivatedCountLastDay = 15,
  kDismissedCountLastDay = 16,
  kSeenCountLast7Days = 17,
  kActivatedCountLast7Days = 18,
  kDismissedCountLast7Days = 19,
  kSeenCountLast30Days = 20,
  kActivatedCountLast30Days = 21,
  kDismissedCountLast30Days = 22,
  kSameTimeGroupVisitCount = 23,
  kSameDayGroupVisitCount = 24,
};

static constexpr size_t kNumInputs = 25;

// Represents a field's metadata and is leveraged for the processing and
// serialization of `URLVisitAggregate` fields participating in ML models.
struct FieldSchema {
  // The enum and index value associated with the given field.
  URLVisitAggregateRankingModelInputSignals signal;
  // The name associated with the given field.
  const char* name;
};

// A collection of relevant fields present in the `URLVisitAggregate` to be
// leveraged for ML use cases.
extern const std::array<FieldSchema, kNumInputs> kURLVisitAggregateSchema;

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_SCHEMA_H_
