// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_TAB_RANK_DISPATCHER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_TAB_RANK_DISPATCHER_H_

#include <map>
#include <queue>
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

namespace segmentation_platform {

// Utility to fetch synced tabs and order them. This is attached to segmentation
// service as user data.
class TabRankDispatcher : public base::SupportsUserData::Data {
 public:
  TabRankDispatcher(SegmentationPlatformService* segmentation_service,
                    sync_sessions::SessionSyncService* session_sync_service,
                    std::unique_ptr<TabFetcher> tab_fetcher);
  ~TabRankDispatcher() override;

  TabRankDispatcher(const TabRankDispatcher&) = delete;
  TabRankDispatcher& operator=(const TabRankDispatcher&) = delete;

  // Wrapper for SessionTab, includes a prediction score.
  struct RankedTab {
    // A tab entry. To access the tab details, use `fetcher()->FindTab(entry)`.
    TabFetcher::TabEntry tab;

    // A score based on the ranking heuristic identified by `segmentation_key`.
    // Higher score is better.
    float model_score = -1;

    bool operator<(const RankedTab& other) const {
      // Rank is lower is score is higher.
      return model_score > other.model_score;
    }
  };

  // Additional filters to apply on the list of tabs.
  struct TabFilter {
    // When the last modified time of the tab is higher than the max value, then
    // tab is excluded from the ranking.
    base::TimeDelta max_tab_age;
  };

  // Fetches a list of ranked tabs for a given feature or ranking heuristic
  // identified by `segmentation_key`. The result is std::multiset, and can be
  // iterated in order of tab rank, from best to worst.
  using RankedTabsCallback =
      base::OnceCallback<void(bool, std::multiset<RankedTab>)>;
  void GetTopRankedTabs(const std::string& segmentation_key,
                        const TabFilter& tab_filter,
                        RankedTabsCallback callback);

  TabFetcher* tab_fetcher() { return tab_fetcher_.get(); }

 private:
  void GetNextResult(const std::string& segmentation_key,
                     std::queue<RankedTab> candidate_tabs,
                     std::multiset<RankedTab> results,
                     RankedTabsCallback callback);
  void OnGetResult(const std::string& segmentation_key,
                   std::queue<RankedTab> candidate_tabs,
                   std::multiset<RankedTab> results,
                   RankedTabsCallback callback,
                   RankedTab current_tab,
                   const AnnotatedNumericResult& result);

  const std::unique_ptr<TabFetcher> tab_fetcher_;

  // Subscribes to the sync session changes. SessionSyncService has a repeating
  // callback to notify of all the session change updates.
  void SubscribeToForeignSessionsChanged();

  // Called every time when the sync session is updated. Using this to record
  // few metrics for sync latency and cross device tabs count.
  void OnForeignSessionUpdated();

  base::Time chrome_startup_timestamp_;
  int session_updated_counter_{0};
  base::CallbackListSubscription foreign_session_updated_subscription_;
  const raw_ptr<SegmentationPlatformService> segmentation_service_;
  const raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;

  base::WeakPtrFactory<TabRankDispatcher> weak_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_TAB_RANK_DISPATCHER_H_
