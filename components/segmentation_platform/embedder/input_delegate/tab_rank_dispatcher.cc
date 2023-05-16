// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"
#include <queue>
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"

namespace segmentation_platform {
namespace {
constexpr uint32_t kTabCandidateLimit = 30;

bool IsTabTooOld(base::Time now,
                 base::Time last_modified_time,
                 base::TimeDelta age_limit) {
  return (now - last_modified_time) > age_limit;
}

}  // namespace

TabRankDispatcher::TabRankDispatcher(
    SegmentationPlatformService* segmentation_service,
    sync_sessions::SessionSyncService* session_sync_service)
    : segmentation_service_(segmentation_service),
      session_sync_service_(session_sync_service) {}

TabRankDispatcher::~TabRankDispatcher() = default;

void TabRankDispatcher::GetTopRankedTabs(const std::string& segmentation_key,
                                         const TabFilter& tab_filter,
                                         RankedTabsCallback callback) {
  if (!session_sync_service_->GetOpenTabsUIDelegate()) {
    std::move(callback).Run(false, {});
    return;
  }
  const sync_sessions::SyncedSession* local_session = nullptr;
  session_sync_service_->GetOpenTabsUIDelegate()->GetLocalSession(
      &local_session);
  std::vector<dangling_raw_ptr<const sync_sessions::SyncedSession>> sessions;
  session_sync_service_->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &sessions);
  if (local_session) {
    sessions.push_back(local_session);
  }
  if (sessions.empty()) {
    std::move(callback).Run(false, {});
    return;
  }

  const base::Time now = base::Time::Now();
  std::queue<RankedTab> candidate_tabs;
  for (const sync_sessions::SyncedSession* session : sessions) {
    if (IsTabTooOld(now, session->GetModifiedTime(), tab_filter.max_tab_age)) {
      continue;
    }
    session->GetSessionTag();
    for (const auto& session_and_window : session->windows) {
      const auto& window = session_and_window.second->wrapped_window;
      for (const auto& tab : window.tabs) {
        if (IsTabTooOld(now, tab->timestamp, tab_filter.max_tab_age)) {
          continue;
        }
        candidate_tabs.push(RankedTab{.tab = tab.get(),
                                      .session_tag = session->GetSessionTag()});
      }
    }
  }
  GetNextResult(segmentation_key, std::move(candidate_tabs),
                std::set<RankedTab>(), std::move(callback));
}

void TabRankDispatcher::GetNextResult(const std::string& segmentation_key,
                                      std::queue<RankedTab> candidate_tabs,
                                      std::set<RankedTab> results,
                                      RankedTabsCallback callback) {
  if (candidate_tabs.empty()) {
    std::move(callback).Run(false, std::move(results));
    return;
  }

  const RankedTab tab = std::move(candidate_tabs.front());
  candidate_tabs.pop();

  PredictionOptions options;
  options.on_demand_execution = true;
  scoped_refptr<InputContext> input_context =
      base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace(
      "session_tag", processing::ProcessedValue(tab.session_tag));
  input_context->metadata_args.emplace(
      "tab_id", processing::ProcessedValue(tab.tab->tab_id.id()));
  segmentation_service_->GetAnnotatedNumericResult(
      segmentation_key, options, input_context,
      base::BindOnce(&TabRankDispatcher::OnGetResult,
                     weak_factory_.GetWeakPtr(), segmentation_key,
                     std::move(candidate_tabs), std::move(results),
                     std::move(callback), std::move(tab)));
}

void TabRankDispatcher::OnGetResult(const std::string& segmentation_key,
                                    std::queue<RankedTab> candidate_tabs,
                                    std::set<RankedTab> results,
                                    RankedTabsCallback callback,
                                    RankedTab current_tab,
                                    const AnnotatedNumericResult& result) {
  if (result.status == PredictionStatus::kSucceeded) {
    current_tab.model_score = *result.GetResultForLabel(segmentation_key);
    results.insert(current_tab);
    if (results.size() > kTabCandidateLimit) {
      results.erase(--results.end());
    }
  }
  GetNextResult(segmentation_key, std::move(candidate_tabs), std::move(results),
                std::move(callback));
}

}  // namespace segmentation_platform
