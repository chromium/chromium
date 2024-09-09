// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"

#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "url/gurl.h"

namespace visited_url_ranking {

using Source = URLVisit::Source;
using URLVisitVariant = URLVisitAggregate::URLVisitVariant;

void AddAggregateVisitDataFromSession(
    const sync_sessions::SyncedSession* session,
    Source source,
    const std::optional<base::Time>& modified_on_or_after_timestamp,
    std::map<URLMergeKey, URLVisitAggregate::TabData>& url_visit_tab_data_map,
    url_deduplication::URLDeduplicationHelper* deduplication_helper) {
  for (const auto& session_and_window : session->windows) {
    const auto& window = session_and_window.second->wrapped_window;
    for (const auto& tab : window.tabs) {
      if (!modified_on_or_after_timestamp.has_value() ||
          tab->timestamp >= modified_on_or_after_timestamp) {
        int selected_index =
            std::min(tab->current_navigation_index,
                     static_cast<int>(tab->navigations.size() - 1));
        const sessions::SerializedNavigationEntry& current_navigation =
            tab->navigations.at(selected_index);
        const GURL& tab_url = current_navigation.virtual_url();
        if (!tab_url.is_valid()) {
          continue;
        }

        auto url_key = ComputeURLMergeKey(tab_url, current_navigation.title(),
                                          deduplication_helper);
        bool tab_data_map_already_has_url_entry =
            url_visit_tab_data_map.find(url_key) !=
            url_visit_tab_data_map.end();
        if (!tab_data_map_already_has_url_entry) {
          auto last_active_tab = URLVisitAggregate::Tab(
              tab->tab_id.id(),
              URLVisit(tab_url, current_navigation.title(), tab->timestamp,
                       session->GetDeviceFormFactor(), source,
                       session->GetSessionName()),
              session->GetSessionTag(), session->GetSessionName());
          auto tab_data =
              URLVisitAggregate::TabData(std::move(last_active_tab));
          tab_data.last_active = tab->last_active_time;
          url_visit_tab_data_map.insert_or_assign(url_key, std::move(tab_data));
        }

        auto& session_tab = url_visit_tab_data_map.at(url_key);
        if (tab_data_map_already_has_url_entry) {
          session_tab.tab_count += 1;
          URLVisitAggregate::Tab& last_active_tab = session_tab.last_active_tab;
          last_active_tab.visit.last_modified =
              std::max(tab->timestamp, last_active_tab.visit.last_modified);
          base::Time current_last_active_time =
              std::max(session_tab.last_active, base::Time::Min());
          if (tab->last_active_time > current_last_active_time) {
            session_tab.last_active_tab = URLVisitAggregate::Tab(
                tab->tab_id.id(),
                URLVisit(tab_url, current_navigation.title(), tab->timestamp,
                         session->GetDeviceFormFactor(), source),
                session->GetSessionTag(), session->GetSessionName());
            session_tab.last_active = tab->last_active_time;
          }
        }

        session_tab.pinned = session_tab.pinned || tab->pinned;
        session_tab.in_group = session_tab.in_group || tab->group.has_value();
      }
    }
  }
}

SessionURLVisitDataFetcher::SessionURLVisitDataFetcher(
    sync_sessions::SessionSyncService* session_sync_service)
    : session_sync_service_(session_sync_service) {}

SessionURLVisitDataFetcher::~SessionURLVisitDataFetcher() = default;

void SessionURLVisitDataFetcher::FetchURLVisitData(
    const FetchOptions& options,
    const FetcherConfig& config,
    FetchResultCallback callback) {
  auto& fetcher_sources = options.fetcher_sources.at(Fetcher::kSession);
  sync_sessions::OpenTabsUIDelegate* open_tabs_ui_delegate =
      session_sync_service_->GetOpenTabsUIDelegate();
  if (!open_tabs_ui_delegate || fetcher_sources.empty()) {
    std::move(callback).Run({FetchResult::Status::kError, {}});
    return;
  }

  // TODO(crbug.com/335200723): Integrate client configurable merging and
  // deduplication strategies provided via `FetchOptions`.
  std::map<URLMergeKey, URLVisitAggregate::TabData> url_visit_tab_data_map;
  if (base::Contains(fetcher_sources, Source::kForeign)) {
    std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
        sessions;
    open_tabs_ui_delegate->GetAllForeignSessions(&sessions);
    for (const sync_sessions::SyncedSession* session : sessions) {
      AddAggregateVisitDataFromSession(
          session, Source::kForeign, options.begin_time, url_visit_tab_data_map,
          config.deduplication_helper);
    }
  }
  if (base::Contains(fetcher_sources, Source::kLocal)) {
    const sync_sessions::SyncedSession* local_session = nullptr;
    open_tabs_ui_delegate->GetLocalSession(&local_session);
    if (local_session) {
      AddAggregateVisitDataFromSession(
          local_session, Source::kLocal, options.begin_time,
          url_visit_tab_data_map, config.deduplication_helper);
    }
  }

  std::map<URLMergeKey, URLVisitVariant> url_visit_variants_map;
  std::transform(
      std::make_move_iterator(url_visit_tab_data_map.begin()),
      std::make_move_iterator(url_visit_tab_data_map.end()),
      std::inserter(url_visit_variants_map, url_visit_variants_map.end()),
      [](auto kv) { return std::make_pair(kv.first, std::move(kv.second)); });
  std::move(callback).Run(
      {FetchResult::Status::kSuccess, std::move(url_visit_variants_map)});
}

}  // namespace visited_url_ranking
