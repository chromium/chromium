// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/desktop_tab_model_url_visit_data_fetcher.h"

#include <map>

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/url_constants.h"

namespace visited_url_ranking {

using Source = URLVisit::Source;
using URLVisitVariant = URLVisitAggregate::URLVisitVariant;

namespace {

URLVisitAggregate::Tab MakeAggregateTabFromWebContents(
    content::WebContents* web_contents) {
  auto* last_entry = web_contents->GetController().GetLastCommittedEntry();
  URLVisitAggregate::Tab tab(
      sessions::SessionTabHelper::IdForTab(web_contents).id(),
      URLVisit(web_contents->GetURL(), web_contents->GetTitle(),
               last_entry->GetTimestamp(),
               syncer::DeviceInfo::FormFactor::kDesktop, Source::kLocal));
  return tab;
}

}  // namespace

DesktopTabModelURLVisitDataFetcher::DesktopTabModelURLVisitDataFetcher(
    Profile* profile)
    : profile_(profile) {}

DesktopTabModelURLVisitDataFetcher::~DesktopTabModelURLVisitDataFetcher() =
    default;

void DesktopTabModelURLVisitDataFetcher::FetchURLVisitData(
    const FetchOptions& options,
    const FetcherConfig& config,
    FetchResultCallback callback) {
  std::map<URLMergeKey, URLVisitAggregate::TabData> url_visit_tab_data_map;
  const BrowserList* browser_list = BrowserList::GetInstance();
  for (const Browser* browser : *browser_list) {
    if (browser->profile() != profile_) {
      continue;
    }

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->GetTabCount(); ++i) {
      auto* web_contents = tab_strip_model->GetWebContentsAt(i);
      auto* last_entry = web_contents->GetController().GetLastCommittedEntry();
      if (!last_entry || last_entry->GetTimestamp() < options.begin_time) {
        continue;
      }
      if (!web_contents->GetURL().SchemeIs(url::kHttpScheme) &&
          !web_contents->GetURL().SchemeIs(url::kHttpsScheme)) {
        continue;
      }

      auto url_key = ComputeURLMergeKey(web_contents->GetLastCommittedURL(),
                                        web_contents->GetTitle(),
                                        config.deduplication_helper);
      auto it = url_visit_tab_data_map.find(url_key);
      bool tab_data_map_already_has_url_entry =
          (it != url_visit_tab_data_map.end());
      base::Time tab_entry_last_active = web_contents->GetLastActiveTime();
      if (!tab_data_map_already_has_url_entry) {
        auto tab_data = URLVisitAggregate::TabData(
            MakeAggregateTabFromWebContents(web_contents));
        tab_data.last_active = tab_entry_last_active;
        url_visit_tab_data_map.insert_or_assign(url_key, std::move(tab_data));
      }

      auto& tab_data = url_visit_tab_data_map.at(url_key);
      if (tab_data_map_already_has_url_entry) {
        if (tab_entry_last_active > tab_data.last_active) {
          tab_data.last_active_tab =
              MakeAggregateTabFromWebContents(web_contents);
          tab_data.last_active = tab_entry_last_active;
        }
        tab_data.tab_count += 1;
      }
      TabRendererData tab_renderer_data =
          TabRendererData::FromTabInModel(tab_strip_model, i);
      tab_data.pinned = tab_data.pinned || tab_renderer_data.pinned;
      tab_data.in_group =
          tab_data.in_group ||
          (tab_strip_model->GetTabGroupForTab(i) != std::nullopt);
    }
  }

  std::map<URLMergeKey, URLVisitVariant> url_visit_variant_map;
  std::transform(
      std::make_move_iterator(url_visit_tab_data_map.begin()),
      std::make_move_iterator(url_visit_tab_data_map.end()),
      std::inserter(url_visit_variant_map, url_visit_variant_map.end()),
      [](auto kv) { return std::make_pair(kv.first, std::move(kv.second)); });

  std::move(callback).Run(
      {FetchResult::Status::kSuccess, std::move(url_visit_variant_map)});
}

}  // namespace visited_url_ranking
