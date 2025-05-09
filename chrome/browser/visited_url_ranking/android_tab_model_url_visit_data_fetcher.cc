// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visited_url_ranking/android_tab_model_url_visit_data_fetcher.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sync_device_info/device_info.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"
#include "components/visited_url_ranking/public/tab_metadata.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "ui/base/device_form_factor.h"

namespace visited_url_ranking {

using Source = URLVisit::Source;
using URLVisitVariant = URLVisitAggregate::URLVisitVariant;

namespace {

TabMetadata::TabOrigin GetTabOriginFromLaunchType(int type) {
  TabModel::TabLaunchType launch_type =
      static_cast<TabModel::TabLaunchType>(type);
  switch (launch_type) {
    case TabModel::TabLaunchType::FROM_LINK:
    case TabModel::TabLaunchType::FROM_EXTERNAL_APP:
    case TabModel::TabLaunchType::FROM_CHROME_UI:
    case TabModel::TabLaunchType::FROM_LONGPRESS_FOREGROUND:
    case TabModel::TabLaunchType::FROM_LONGPRESS_BACKGROUND:
    case TabModel::TabLaunchType::FROM_REPARENTING:
    case TabModel::TabLaunchType::FROM_LAUNCHER_SHORTCUT:
    case TabModel::TabLaunchType::FROM_LAUNCH_NEW_INCOGNITO_TAB:
    case TabModel::TabLaunchType::FROM_TAB_GROUP_UI:
    case TabModel::TabLaunchType::FROM_LONGPRESS_BACKGROUND_IN_GROUP:
    case TabModel::TabLaunchType::FROM_APP_WIDGET:
    case TabModel::TabLaunchType::FROM_LONGPRESS_INCOGNITO:
    case TabModel::TabLaunchType::FROM_RECENT_TABS:
    case TabModel::TabLaunchType::FROM_READING_LIST:
    case TabModel::TabLaunchType::FROM_TAB_SWITCHER_UI:
    case TabModel::TabLaunchType::FROM_OMNIBOX:
    case TabModel::TabLaunchType::FROM_BOOKMARK_BAR_BACKGROUND:
    case TabModel::TabLaunchType::FROM_RECENT_TABS_FOREGROUND:
    case TabModel::TabLaunchType::FROM_HISTORY_NAVIGATION_BACKGROUND:
    case TabModel::TabLaunchType::FROM_HISTORY_NAVIGATION_FOREGROUND:
    case TabModel::TabLaunchType::FROM_LONGPRESS_FOREGROUND_IN_GROUP:
      return TabMetadata::TabOrigin::kOpenedByUserAction;

    case TabModel::TabLaunchType::FROM_RESTORE:
    case TabModel::TabLaunchType::FROM_SPECULATIVE_BACKGROUND_CREATION:
    case TabModel::TabLaunchType::FROM_BROWSER_ACTIONS:
    case TabModel::TabLaunchType::FROM_STARTUP:
    case TabModel::TabLaunchType::FROM_START_SURFACE:
    case TabModel::TabLaunchType::FROM_RESTORE_TABS_UI:
    case TabModel::TabLaunchType::UNSET:
    case TabModel::TabLaunchType::FROM_SYNC_BACKGROUND:
    case TabModel::TabLaunchType::FROM_COLLABORATION_BACKGROUND_IN_GROUP:
    case TabModel::TabLaunchType::FROM_REPARENTING_BACKGROUND:
      return TabMetadata::TabOrigin::kOpenedWithoutUserAction;

    case TabModel::TabLaunchType::SIZE:
      NOTREACHED();
  }
}

URLVisitAggregate::Tab MakeAggregateTab(
    const TabModel* tab_model,
    const TabAndroid* tab_android,
    syncer::DeviceInfo::FormFactor form_factor) {
  auto tab = URLVisitAggregate::Tab(
      tab_android->GetAndroidId(),
      URLVisit(tab_android->GetURL(), tab_android->GetTitle(),
               tab_android->GetLastShownTimestamp(), form_factor,
               Source::kLocal));
  if (!base::FeatureList::IsEnabled(features::kGroupSuggestionService)) {
    return tab;
  }
  tab.tab_metadata.is_currently_active =
      tab_model->IsActiveModel() &&
      tab_model->GetTabAt(tab_model->GetActiveIndex()) == tab_android;
  tab.tab_metadata.tab_android_launch_type =
      tab_android->GetTabLaunchTypeAtCreation();
  tab.tab_metadata.tab_origin =
      GetTabOriginFromLaunchType(tab.tab_metadata.tab_android_launch_type);
  tab.tab_metadata.parent_tab_id = tab_android->GetParentId();
  std::optional<tab_groups::TabGroupId> tab_group_id = tab_android->GetGroup();
  // Use optional::transform once C++23 is allowed.
  tab.tab_metadata.local_tab_group_id =
      tab_group_id.has_value() ? std::make_optional(tab_group_id->token())
                               : std::nullopt;
  auto* web_contents = tab_android->GetContents();
  if (!web_contents || !web_contents->GetPrimaryMainFrame()) {
    tab.tab_metadata.ukm_source_id = ukm::kInvalidSourceId;
  } else {
    tab.tab_metadata.ukm_source_id =
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  }
  for (int i = 0; i < tab_model->GetTabCount(); ++i) {
    if (tab_model->GetTabAt(i) == tab_android) {
      tab.tab_metadata.tab_model_index = i;
      break;
    }
  }
  tab.tab_metadata.is_last_tab_in_tab_model =
      tab.tab_metadata.tab_model_index == tab_model->GetTabCount() - 1;
  return tab;
}

}  // namespace

AndroidTabModelURLVisitDataFetcher::AndroidTabModelURLVisitDataFetcher(
    Profile* profile)
    : profile_(profile) {}

AndroidTabModelURLVisitDataFetcher::~AndroidTabModelURLVisitDataFetcher() =
    default;

void AndroidTabModelURLVisitDataFetcher::FetchURLVisitData(
    const FetchOptions& options,
    const FetcherConfig& config,
    FetchResultCallback callback) {
  std::map<URLMergeKey, URLVisitAggregate::TabData> url_visit_tab_data_map;
  syncer::DeviceInfo::FormFactor form_factor =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? syncer::DeviceInfo::FormFactor::kTablet
          : syncer::DeviceInfo::FormFactor::kPhone;
  for (TabModel* model : TabModelList::models()) {
    if (profile_ != model->GetProfile()) {
      continue;
    }

    int count = model->GetTabCount();
    for (int i = 0; i < count; ++i) {
      auto* tab_android = model->GetTabAt(i);
      GURL url = tab_android->GetURL();
      if (!url.is_valid() || url.spec() == chrome::kChromeUINativeNewTabURL) {
        continue;
      }

      auto last_show_timestamp = tab_android->GetLastShownTimestamp();
      if (last_show_timestamp < options.begin_time) {
        continue;
      }

      auto url_key = ComputeURLMergeKey(url, tab_android->GetTitle(),
                                        config.deduplication_helper);
      bool tab_data_map_already_has_url_entry =
          (url_visit_tab_data_map.find(url_key) !=
           url_visit_tab_data_map.end());
      if (!tab_data_map_already_has_url_entry) {
        url_visit_tab_data_map.emplace(
            url_key, MakeAggregateTab(model, tab_android, form_factor));
      }

      auto& tab_data = url_visit_tab_data_map.at(url_key);
      if (tab_data_map_already_has_url_entry) {
        if (tab_data.last_active_tab.visit.last_modified <
            last_show_timestamp) {
          tab_data.last_active_tab =
              MakeAggregateTab(model, tab_android, form_factor);
        }
        ++tab_data.tab_count;
      }

      tab_data.last_active =
          std::max(tab_data.last_active, last_show_timestamp);
      // Not applicable to android.
      tab_data.pinned = false;
      tab_data.in_group = tab_data.in_group || tab_android->GetGroup();
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
