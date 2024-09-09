// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>

namespace visited_url_ranking {

URLVisit::URLVisit(const GURL& url_arg,
                   const std::u16string& title_arg,
                   const base::Time& last_modified_arg,
                   syncer::DeviceInfo::FormFactor device_type_arg,
                   Source source_arg,
                   const std::optional<std::string>& client_name_arg)
    : url(url_arg),
      title(title_arg),
      last_modified(last_modified_arg),
      device_type(device_type_arg),
      source(source_arg),
      client_name(client_name_arg) {}

URLVisit::URLVisit(const URLVisit&) = default;

URLVisit::~URLVisit() = default;

URLVisitAggregate::URLVisitAggregate(std::string key_arg) : url_key(key_arg) {
  DCHECK(!url_key.empty());
}

URLVisitAggregate::~URLVisitAggregate() = default;

URLVisitAggregate::URLVisitAggregate(URLVisitAggregate&& other) = default;

URLVisitAggregate& URLVisitAggregate::operator=(URLVisitAggregate&& other) =
    default;

std::set<std::u16string_view> URLVisitAggregate::GetAssociatedTitles() const {
  std::set<std::u16string_view> titles = {};
  for (const auto& fetcher_entry : fetcher_data_map) {
    std::visit(
        URLVisitVariantHelper{
            [&titles](const URLVisitAggregate::TabData& tab_data) {
              titles.insert(tab_data.last_active_tab.visit.title);
            },
            [&titles](const URLVisitAggregate::HistoryData& history_data) {
              titles.insert(history_data.last_visited.url_row.title());
            }},
        fetcher_entry.second);
  }
  return titles;
}

std::set<const GURL*> URLVisitAggregate::GetAssociatedURLs() const {
  std::set<const GURL*> urls = {};
  for (const auto& fetcher_entry : fetcher_data_map) {
    std::visit(URLVisitVariantHelper{
                   [&urls](const URLVisitAggregate::TabData& tab_data) {
                     urls.insert(&tab_data.last_active_tab.visit.url);
                   },
                   [&urls](const URLVisitAggregate::HistoryData& history_data) {
                     urls.insert(&history_data.last_visited.url_row.url());
                   }},
               fetcher_entry.second);
  }
  return urls;
}

base::Time URLVisitAggregate::GetLastVisitTime() const {
  std::optional<base::Time> last_visit_time;
  for (const auto& fetcher_entry : fetcher_data_map) {
    switch (fetcher_entry.first) {
      case Fetcher::kTabModel: {
        std::optional<base::Time> tab_last_active =
            std::get<URLVisitAggregate::TabData>(fetcher_entry.second)
                .last_active;
        if (!last_visit_time ||
            (tab_last_active && last_visit_time < tab_last_active)) {
          last_visit_time = tab_last_active;
        } else if (!last_visit_time && !tab_last_active) {
          last_visit_time =
              std::get<URLVisitAggregate::TabData>(fetcher_entry.second)
                  .last_active_tab.visit.last_modified;
        }
        break;
      }
      case Fetcher::kSession: {
        std::optional<base::Time> session_last_active =
            std::get<URLVisitAggregate::TabData>(fetcher_entry.second)
                .last_active;
        if (!last_visit_time ||
            (session_last_active && last_visit_time < session_last_active)) {
          last_visit_time = session_last_active;
        } else if (!last_visit_time && !session_last_active) {
          last_visit_time =
              std::get<URLVisitAggregate::TabData>(fetcher_entry.second)
                  .last_active_tab.visit.last_modified;
        }
        break;
      }
      case Fetcher::kHistory: {
        std::optional<base::Time> history_last_visit =
            std::get<URLVisitAggregate::HistoryData>(fetcher_entry.second)
                .last_visited.visit_row.visit_time;
        if (!last_visit_time ||
            (history_last_visit && last_visit_time < history_last_visit)) {
          last_visit_time = history_last_visit;
        }
        break;
      }
    }
  }
  return *last_visit_time;
}

URLVisitAggregate::Tab::Tab(const int32_t id_arg,
                            URLVisit visit_arg,
                            std::optional<std::string> session_tag_arg,
                            std::optional<std::string> session_name_arg)
    : id(id_arg),
      visit(std::move(visit_arg)),
      session_tag(session_tag_arg),
      session_name(session_name_arg) {}

URLVisitAggregate::Tab::Tab(const URLVisitAggregate::Tab&) = default;

URLVisitAggregate::Tab::~Tab() = default;

URLVisitAggregate::TabData::TabData(Tab last_active_tab_arg)
    : last_active_tab(std::move(last_active_tab_arg)) {}

URLVisitAggregate::TabData::TabData(const URLVisitAggregate::TabData&) =
    default;

URLVisitAggregate::TabData::~TabData() = default;

URLVisitAggregate::HistoryData::HistoryData(
    history::AnnotatedVisit annotated_visit,
    std::optional<std::string> client_name,
    syncer::DeviceInfo::FormFactor device_type)
    : last_visited(std::move(annotated_visit)),
      visit(last_visited.url_row.url(),
            last_visited.url_row.title(),
            last_visited.visit_row.visit_time,
            device_type,
            last_visited.visit_row.originator_cache_guid.empty()
                ? URLVisit::Source::kLocal
                : URLVisit::Source::kForeign,
            std::move(client_name)) {
  if (last_visited.context_annotations.total_foreground_duration
          .InMilliseconds() > 0) {
    total_foreground_duration =
        last_visited.context_annotations.total_foreground_duration;
  }

  if (last_visited.visit_row.app_id.has_value()) {
    last_app_id = last_visited.visit_row.app_id;
  }
}

URLVisitAggregate::HistoryData::HistoryData(
    URLVisitAggregate::HistoryData&& other) = default;

URLVisitAggregate::HistoryData& URLVisitAggregate::HistoryData::operator=(
    URLVisitAggregate::HistoryData&& other) = default;

URLVisitAggregate::HistoryData::~HistoryData() = default;

}  // namespace visited_url_ranking
