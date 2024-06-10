// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit_schema.h"

#include <array>

namespace visited_url_ranking {

const char kSignalTimeSinceModifiedSec[] = "time_since_last_modified_sec";
const char kSignalTimeSinceLastActiveSec[] = "time_since_last_active_sec";
const char kSignalTimeActiveForTimePeriodSec[] =
    "time_active_for_time_period_sec";
const char kSignalTabCount[] = "tab_count";
const char kSignalVisitCount[] = "visit_count";
const char kSignalNumTimesActive[] = "num_times_active";
const char kSignalIsBookmarked[] = "is_bookmarked";
const char kSignalIsPinned[] = "is_pinned";
const char kSignalIsInTabGroup[] = "is_in_tab_group";
const char kSignalIsInCluster[] = "is_in_cluster";
const char kSignalHasUrlKeyedImage[] = "has_url_keyed_image";
const char kSignalHasAppId[] = "has_app_id";
const char kPlatformInputId[] = "platform_type";

constexpr std::array<FieldSchema, kNumInputs> kURLVisitAggregateSchema = {{
    {.signal =
         URLVisitAggregateRankingModelInputSignals::kTimeSinceLastModifiedSec,
     .name = kSignalTimeSinceModifiedSec},
    {.signal =
         URLVisitAggregateRankingModelInputSignals::kTimeSinceLastActiveSec,
     .name = kSignalTimeSinceLastActiveSec},
    {.signal =
         URLVisitAggregateRankingModelInputSignals::kTimeActiveForTimePeriodSec,
     .name = kSignalTimeActiveForTimePeriodSec},
    {.signal = URLVisitAggregateRankingModelInputSignals::kNumTimesActive,
     .name = kSignalNumTimesActive},
    {.signal = URLVisitAggregateRankingModelInputSignals::kTabCount,
     .name = kSignalTabCount},
    {.signal = URLVisitAggregateRankingModelInputSignals::kVisitCount,
     .name = kSignalVisitCount},
    {.signal = URLVisitAggregateRankingModelInputSignals::kIsBookmarked,
     .name = kSignalIsBookmarked},
    {.signal = URLVisitAggregateRankingModelInputSignals::kIsPinned,
     .name = kSignalIsPinned},
    {.signal = URLVisitAggregateRankingModelInputSignals::kIsInTabGroup,
     .name = kSignalIsInTabGroup},
    {.signal = URLVisitAggregateRankingModelInputSignals::kIsInCluster,
     .name = kSignalIsInCluster},
    {.signal = URLVisitAggregateRankingModelInputSignals::kHasUrlKeyedImage,
     .name = kSignalHasUrlKeyedImage},
    {.signal = URLVisitAggregateRankingModelInputSignals::kHasAppId,
     .name = kSignalHasAppId},
    {.signal = URLVisitAggregateRankingModelInputSignals::kPlatform,
     .name = kPlatformInputId},
}};

}  // namespace visited_url_ranking
