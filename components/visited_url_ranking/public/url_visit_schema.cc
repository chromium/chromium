// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit_schema.h"

#include <array>
#include <utility>

namespace visited_url_ranking {

const char kSignalTimeSinceModifiedSec[] = "time_since_last_modified_sec";
const char kSignalTimeSinceLastActiveSec[] = "time_since_last_active_sec";
const char kSignalTimeActiveForTimePeriodSec[] =
    "time_active_for_time_period_sec";
const char kSignalLocalTabCount[] = "local_tab_count";
const char kSignalSessionTabCount[] = "session_tab_count";
const char kSignalVisitCount[] = "visit_count";
const char kSignalNumTimesActive[] = "num_times_active";
const char kSignalIsBookmarked[] = "is_bookmarked";
const char kSignalIsPinned[] = "is_pinned";
const char kSignalIsInTabGroup[] = "is_in_tab_group";
const char kSignalIsInCluster[] = "is_in_cluster";
const char kSignalHasUrlKeyedImage[] = "has_url_keyed_image";
const char kSignalHasAppId[] = "has_app_id";
const char kPlatformInputId[] = "platform_type";
const char kSignalSeenCountLastDay[] = "seen_count_last_day";
const char kSignalActivatedCountLastDay[] = "activated_count_last_day";
const char kSignalDismissedCountLastDay[] = "dismissed_count_last_day";
const char kSignalSeenCountLast7Days[] = "seen_count_last_7_days";
const char kSignalActivatedCountLast7Days[] = "activated_count_last_7_days";
const char kSignalDismissedCountLast7Days[] = "dismissed_count_last_7_days";
const char kSignalSeenCountLast30Days[] = "seen_count_last_30_days";
const char kSignalActivatedCountLast30Days[] = "activated_count_last_30_days";
const char kSignalDismissedCountLast30Days[] = "dismissed_count_last_30_days";
const char kSignalSameTimeGroupVisitCount[] = "same_time_group_visit_count";
const char kSignalSameDayGroupVisitCount[] = "same_day_group_visit_count";
const char kSignalTabRecentForegroundCount[] = "tab_recent_foreground_count";
const char kSignalIsTabOpenedByUser[] = "is_tab_opened_by_user";
const char kSignalAndroidTabLaunchType[] = "android_tab_launch_type";
const char kSignalAndroidTabLaunchPackageName[] =
    "android_tab_launch_package_name";
const char kSignalTabParentId[] = "tab_parent_id";
const char kSignalTimeSinceTabCreationSec[] = "time_since_tab_creation_sec";
const char kSignalTabGroupSyncId[] = "tab_group_sync_id";
const char kSignalTabId[] = "tab_id";
const char kSignalTabUrlOriginHash[] = "tab_url_origin_hash";
const char kSignalTabUkmSourceId[] = "tab_ukm_source_id";
const char kSignalIsTabSelected[] = "is_tab_selected";
const char kSignalTabIndex[] = "tab_index";
const char kSignalIsLastTab[] = "is_last_tab";

constexpr std::array<FieldSchema, kTabResumptionNumInputs>
    kURLVisitAggregateSchema = {{
        {.signal = URLVisitAggregateRankingModelInputSignals::
             kTimeSinceLastModifiedSec,
         .name = kSignalTimeSinceModifiedSec},
        {.signal =
             URLVisitAggregateRankingModelInputSignals::kTimeSinceLastActiveSec,
         .name = kSignalTimeSinceLastActiveSec},
        {.signal = URLVisitAggregateRankingModelInputSignals::
             kTimeActiveForTimePeriodSec,
         .name = kSignalTimeActiveForTimePeriodSec},
        {.signal = URLVisitAggregateRankingModelInputSignals::kNumTimesActive,
         .name = kSignalNumTimesActive},
        {.signal = URLVisitAggregateRankingModelInputSignals::kLocalTabCount,
         .name = kSignalLocalTabCount},
        {.signal = URLVisitAggregateRankingModelInputSignals::kSessionTabCount,
         .name = kSignalSessionTabCount},
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
        {.signal = URLVisitAggregateRankingModelInputSignals::kSeenCountLastDay,
         .name = kSignalSeenCountLastDay},
        {.signal =
             URLVisitAggregateRankingModelInputSignals::kActivatedCountLastDay,
         .name = kSignalActivatedCountLastDay},
        {.signal =
             URLVisitAggregateRankingModelInputSignals::kDismissedCountLastDay,
         .name = kSignalDismissedCountLastDay},
        {.signal =
             URLVisitAggregateRankingModelInputSignals::kSeenCountLast7Days,
         .name = kSignalSeenCountLast7Days},
        {.signal = URLVisitAggregateRankingModelInputSignals::
             kActivatedCountLast7Days,
         .name = kSignalActivatedCountLast7Days},
        {.signal = URLVisitAggregateRankingModelInputSignals::
             kDismissedCountLast7Days,
         .name = kSignalDismissedCountLast7Days},
        {.signal =
             URLVisitAggregateRankingModelInputSignals::kSeenCountLast30Days,
         .name = kSignalSeenCountLast30Days},
        {.signal = URLVisitAggregateRankingModelInputSignals::
             kActivatedCountLast30Days,
         .name = kSignalActivatedCountLast30Days},
        {.signal = URLVisitAggregateRankingModelInputSignals::
             kDismissedCountLast30Days,
         .name = kSignalDismissedCountLast30Days},
        {.signal = URLVisitAggregateRankingModelInputSignals::
             kSameTimeGroupVisitCount,
         .name = kSignalSameTimeGroupVisitCount},
        {.signal =
             URLVisitAggregateRankingModelInputSignals::kSameDayGroupVisitCount,
         .name = kSignalSameDayGroupVisitCount},
    }};

constexpr std::array<FieldSchema, kSuggestionsNumInputs>
    kSuggestionsPredictionSchema{
        {{.signal = URLVisitAggregateRankingModelInputSignals::
              kTimeSinceLastModifiedSec,
          .name = kSignalTimeSinceModifiedSec},
         {.signal = URLVisitAggregateRankingModelInputSignals::
              kTimeSinceLastActiveSec,
          .name = kSignalTimeSinceLastActiveSec},
         {.signal = URLVisitAggregateRankingModelInputSignals::
              kTabRecentForegroundCount,
          .name = kSignalTabRecentForegroundCount},
         {.signal =
              URLVisitAggregateRankingModelInputSignals::kIsTabOpenedByUser,
          .name = kSignalIsTabOpenedByUser},
         {.signal =
              URLVisitAggregateRankingModelInputSignals::kAndroidTabLaunchType,
          .name = kSignalAndroidTabLaunchType},
         {.signal = URLVisitAggregateRankingModelInputSignals::
              kAndroidTabLaunchPackageName,
          .name = kSignalAndroidTabLaunchPackageName},
         {.signal = URLVisitAggregateRankingModelInputSignals::kTabParentId,
          .name = kSignalTabParentId},
         {.signal = URLVisitAggregateRankingModelInputSignals::
              kTimeSinceTabCreationSec,
          .name = kSignalTimeSinceTabCreationSec},
         {.signal = URLVisitAggregateRankingModelInputSignals::kTabGroupSyncId,
          .name = kSignalTabGroupSyncId},
         {.signal = URLVisitAggregateRankingModelInputSignals::kTabId,
          .name = kSignalTabId},
         {.signal =
              URLVisitAggregateRankingModelInputSignals::kTabUrlOriginHash,
          .name = kSignalTabUrlOriginHash},
         {.signal = URLVisitAggregateRankingModelInputSignals::kTabUkmSourceId,
          .name = kSignalTabUkmSourceId},
         {.signal = URLVisitAggregateRankingModelInputSignals::kIsTabSelected,
          .name = kSignalIsTabSelected},
         {.signal = URLVisitAggregateRankingModelInputSignals::kTabIndex,
          .name = kSignalTabIndex},
         {.signal = URLVisitAggregateRankingModelInputSignals::kIsLastTab,
          .name = kSignalIsLastTab}}};

const char* GetNameForInput(URLVisitAggregateRankingModelInputSignals signal) {
  for (const auto& field : kSuggestionsPredictionSchema) {
    if (field.signal == signal) {
      return field.name;
    }
  }
  return nullptr;
}

}  // namespace visited_url_ranking
