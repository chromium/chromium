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
  kTabCount = 4,
  kVisitCount = 5,
  kIsBookmarked = 6,
  kIsPinned = 7,
  kIsInTabGroup = 8,
  kIsInCluster = 9,
  kHasUrlKeyedImage = 10,
  kHasAppId = 11,
  kPlatform = 12,
};

static constexpr size_t kNumInputs = 13;

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
