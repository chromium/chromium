// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"
#include <memory>
#include <queue>
#include <vector>
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform {
namespace {
constexpr uint32_t kTabCandidateLimit = 30;

void RecordDelayFromStartupToFirstSyncUpdate(
    base::TimeDelta sync_delay_duration) {
  base::UmaHistogramLongTimes100(
      "SegmentationPlatform.SyncSessions.TimeFromStartupToFirstSyncUpdate",
      sync_delay_duration);
}

void RecordDelayFromStartupToSyncUpdate(base::TimeDelta sync_delay_duration) {
  base::UmaHistogramLongTimes100(
      "SegmentationPlatform.SyncSessions.TimeFromStartupToSyncUpdate",
      sync_delay_duration);
}

void RecordDelayFromTabLoadToSyncUpdate(base::TimeDelta sync_delay_duration) {
  base::UmaHistogramLongTimes100(
      "SegmentationPlatform.SyncSessions.TimeFromTabLoadedToSyncUpdate",
      sync_delay_duration);
}

void RecordTabCountAtStartup(long cross_device_tab_count) {
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.SyncSessions.TabsCountAtStartup",
      cross_device_tab_count);
}

void RecordTabCountFromStartupToFirstSyncUpdate(long cross_device_tab_count) {
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.SyncSessions.TotalTabCountAtFirstSyncUpdate",
      cross_device_tab_count);
}

void RecordRecent1HourTabCountAtFirstSyncUpdate(long cross_device_tab_count) {
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.SyncSessions.Recent1HourTabCountAtFirstSyncUpdate",
      cross_device_tab_count);
}

void RecordRecent1DayTabCountAtFirstSyncUpdate(long cross_device_tab_count) {
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.SyncSessions.Recent1DayTabCountAtFirstSyncUpdate",
      cross_device_tab_count);
}

void RecordTabCountAtSyncUpdate(long cross_device_tab_count) {
  base::UmaHistogramCounts1000(
      "SegmentationPlatform.SyncSessions.RecordTabCountAtSyncUpdate",
      cross_device_tab_count);
}

}  // namespace

TabRankDispatcher::TabRankDispatcher(
    SegmentationPlatformService* segmentation_service,
    sync_sessions::SessionSyncService* session_sync_service,
    std::unique_ptr<TabFetcher> tab_fetcher)
    : tab_fetcher_(std::move(tab_fetcher)),
      chrome_startup_timestamp_(base::Time::Now()),
      segmentation_service_(segmentation_service),
      session_sync_service_(session_sync_service) {
  RecordTabCountAtStartup(
      tab_fetcher_->GetRemoteTabsCountAfterTime(base::Time()));

  SubscribeToForeignSessionsChanged();
}

TabRankDispatcher::~TabRankDispatcher() = default;

void TabRankDispatcher::GetTopRankedTabs(const std::string& segmentation_key,
                                         const TabFilter& tab_filter,
                                         RankedTabsCallback callback) {
  std::vector<TabFetcher::TabEntry> all_tabs;
  tab_fetcher_->FillAllRemoteTabs(all_tabs);
  tab_fetcher_->FillAllLocalTabs(all_tabs);
  if (all_tabs.empty()) {
    std::move(callback).Run(false, {});
    return;
  }

  std::queue<RankedTab> candidate_tabs;
  for (const auto& tab : all_tabs) {
    TabFetcher::Tab fetched_tab = tab_fetcher_->FindTab(tab);
    if (!tab_filter.is_null() && !tab_filter.Run(fetched_tab)) {
      continue;
    }
    candidate_tabs.push(RankedTab{.tab = tab});
  }
  GetNextResult(segmentation_key, std::move(candidate_tabs),
                std::multiset<RankedTab>(), std::move(callback));
}

void TabRankDispatcher::GetNextResult(const std::string& segmentation_key,
                                      std::queue<RankedTab> candidate_tabs,
                                      std::multiset<RankedTab> results,
                                      RankedTabsCallback callback) {
  if (candidate_tabs.empty()) {
    std::move(callback).Run(true, std::move(results));
    return;
  }

  RankedTab tab = std::move(candidate_tabs.front());
  // Fetch tab every time from the `tab_fetcher_` for accessing the tab data
  // since the tab could have been destroyed.
  TabFetcher::Tab fetched_tab = tab_fetcher_->FindTab(tab.tab);
  candidate_tabs.pop();

  PredictionOptions options;
  options.on_demand_execution = true;
  scoped_refptr<InputContext> input_context =
      base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace(
      "session_tag", processing::ProcessedValue(tab.tab.session_tag));
  input_context->metadata_args.emplace(
      "tab_id", processing::ProcessedValue(tab.tab.tab_id.id()));
  input_context->metadata_args.emplace(
      "origin", processing::ProcessedValue(
                    fetched_tab.tab_url.DeprecatedGetOriginAsURL()));
  segmentation_service_->GetAnnotatedNumericResult(
      segmentation_key, options, input_context,
      base::BindOnce(&TabRankDispatcher::OnGetResult,
                     weak_factory_.GetWeakPtr(), segmentation_key,
                     std::move(candidate_tabs), std::move(results),
                     std::move(callback), std::move(tab)));
}

void TabRankDispatcher::OnGetResult(const std::string& segmentation_key,
                                    std::queue<RankedTab> candidate_tabs,
                                    std::multiset<RankedTab> results,
                                    RankedTabsCallback callback,
                                    RankedTab current_tab,
                                    const AnnotatedNumericResult& result) {
  if (result.status == PredictionStatus::kSucceeded) {
    current_tab.model_score = *result.GetResultForLabel(segmentation_key);
    current_tab.request_id = result.request_id;
    results.insert(current_tab);
    if (results.size() > kTabCandidateLimit) {
      results.erase(--results.end());
    }
  }
  GetNextResult(segmentation_key, std::move(candidate_tabs), std::move(results),
                std::move(callback));
}

void TabRankDispatcher::SubscribeToForeignSessionsChanged() {
  foreign_session_updated_subscription_ =
      session_sync_service_->SubscribeToForeignSessionsChanged(
          base::BindRepeating(&TabRankDispatcher::OnForeignSessionUpdated,
                              weak_factory_.GetWeakPtr()));
}

void TabRankDispatcher::OnForeignSessionUpdated() {
  base::Time foreign_session_updated_time = base::Time::Now();
  RecordTabCountAtSyncUpdate(
      tab_fetcher_->GetRemoteTabsCountAfterTime(base::Time()));

  std::optional<base::Time> sync_session_modified_timestamp =
      tab_fetcher_->GetLatestRemoteSessionModifiedTime();

  if (!sync_session_modified_timestamp.has_value()) {
    return;
  }
  if (session_updated_counter_ == 0 &&
      chrome_startup_timestamp_ > sync_session_modified_timestamp.value()) {
    // Delay Metrics.
    RecordDelayFromStartupToFirstSyncUpdate(foreign_session_updated_time -
                                            chrome_startup_timestamp_);
    // Tab Count Metrics.
    RecordTabCountFromStartupToFirstSyncUpdate(
        tab_fetcher_->GetRemoteTabsCountAfterTime(base::Time()));
    RecordRecent1HourTabCountAtFirstSyncUpdate(
        tab_fetcher_->GetRemoteTabsCountAfterTime(foreign_session_updated_time -
                                                  base::Hours(1)));
    RecordRecent1DayTabCountAtFirstSyncUpdate(
        tab_fetcher_->GetRemoteTabsCountAfterTime(foreign_session_updated_time -
                                                  base::Days(1)));

  } else if (chrome_startup_timestamp_ >
             sync_session_modified_timestamp.value()) {
    RecordDelayFromStartupToSyncUpdate(foreign_session_updated_time -
                                       chrome_startup_timestamp_);
  } else {
    RecordDelayFromTabLoadToSyncUpdate(foreign_session_updated_time -
                                       sync_session_modified_timestamp.value());
  }
  session_updated_counter_++;
}

}  // namespace segmentation_platform
