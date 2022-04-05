// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_list.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "url/gurl.h"

namespace {

void Clone(const apps::PreferredAppsList::PreferredApps& source,
           apps::PreferredAppsList::PreferredApps* destination) {
  destination->clear();
  for (auto& preferred_app : source) {
    destination->push_back(preferred_app->Clone());
  }
}

}  // namespace

namespace apps {

PreferredAppsList::PreferredAppsList() = default;
PreferredAppsList::~PreferredAppsList() = default;

void PreferredAppsList::Init() {
  preferred_apps_ = PreferredApps();
  initialized_ = true;
}

void PreferredAppsList::Init(PreferredApps& preferred_apps) {
  Clone(preferred_apps, &preferred_apps_);
  auto iter = preferred_apps_.begin();
  while (iter != preferred_apps_.end()) {
    if (apps_util::IsSupportedLinkForApp((*iter)->app_id,
                                         (*iter)->intent_filter)) {
      for (auto& obs : observers_) {
        obs.OnPreferredAppChanged((*iter)->app_id, true);
      }
    }
    iter++;
  }
  initialized_ = true;
}

apps::mojom::ReplacedAppPreferencesPtr PreferredAppsList::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  auto replaced_app_preferences = apps::mojom::ReplacedAppPreferences::New();

  if (EntryExists(app_id, intent_filter)) {
    return replaced_app_preferences;
  }

  auto iter = preferred_apps_.begin();
  auto& replaced_preference_map = replaced_app_preferences->replaced_preference;

  // Go through the list and see if there are overlapped intent filters in the
  // list. If there is, add this into the replaced_app_preferences and remove it
  // from the list.
  while (iter != preferred_apps_.end()) {
    // Only replace overlapped intent filters for other apps.
    if ((*iter)->app_id != app_id &&
        apps_util::FiltersHaveOverlap((*iter)->intent_filter, intent_filter)) {
      // Add the to be removed preferred app into a map, key by app_id.
      replaced_preference_map[(*iter)->app_id].push_back(
          std::move((*iter)->intent_filter));
      iter = preferred_apps_.erase(iter);
    } else {
      iter++;
    }
  }
  auto new_preferred_app =
      apps::mojom::PreferredApp::New(intent_filter->Clone(), app_id);
  preferred_apps_.push_back(std::move(new_preferred_app));

  if (apps_util::IsSupportedLinkForApp(app_id, intent_filter)) {
    for (auto& obs : observers_) {
      obs.OnPreferredAppChanged(app_id, true);
      for (auto& app : replaced_preference_map) {
        obs.OnPreferredAppChanged(app.first, false);
      }
    }
  }
  return replaced_app_preferences;
}

std::vector<apps::mojom::IntentFilterPtr> PreferredAppsList::DeletePreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  // Go through the list and see if there are overlapped intent filters with the
  // same app id in the list. If there are, delete the entry.
  std::vector<apps::mojom::IntentFilterPtr> out;
  auto iter = preferred_apps_.begin();
  while (iter != preferred_apps_.end()) {
    if ((*iter)->app_id == app_id &&
        apps_util::FiltersHaveOverlap((*iter)->intent_filter, intent_filter)) {
      out.push_back(std::move((*iter)->intent_filter));
      iter = preferred_apps_.erase(iter);
    } else {
      iter++;
    }
  }

  if (apps_util::IsSupportedLinkForApp(app_id, intent_filter)) {
    for (auto& obs : observers_) {
      obs.OnPreferredAppChanged(app_id, false);
    }
  }

  return out;
}

std::vector<apps::mojom::IntentFilterPtr> PreferredAppsList::DeleteAppId(
    const std::string& app_id) {
  std::vector<apps::mojom::IntentFilterPtr> out;

  auto iter = preferred_apps_.begin();
  // Go through the list and delete the entry with requested app_id.
  while (iter != preferred_apps_.end()) {
    if ((*iter)->app_id == app_id) {
      out.push_back(std::move((*iter)->intent_filter));
      iter = preferred_apps_.erase(iter);
    } else {
      iter++;
    }
  }

  for (auto& obs : observers_) {
    obs.OnPreferredAppChanged(app_id, false);
  }

  return out;
}

std::vector<apps::mojom::IntentFilterPtr>
PreferredAppsList::DeleteSupportedLinks(const std::string& app_id) {
  std::vector<apps::mojom::IntentFilterPtr> out;

  auto iter = preferred_apps_.begin();
  while (iter != preferred_apps_.end()) {
    if ((*iter)->app_id == app_id &&
        apps_util::IsSupportedLinkForApp(app_id, (*iter)->intent_filter)) {
      out.push_back(std::move((*iter)->intent_filter));
      iter = preferred_apps_.erase(iter);
    } else {
      iter++;
    }
  }

  if (!out.empty()) {
    for (auto& obs : observers_) {
      obs.OnPreferredAppChanged(app_id, false);
    }
  }

  return out;
}

void PreferredAppsList::ApplyBulkUpdate(
    apps::mojom::PreferredAppChangesPtr changes) {
  // Process removed filters first. There's no difference in behavior whether we
  // handle removals or additions first, but doing removals first means there
  // are fewer items in the list to search through when finding matches.
  for (const auto& removed_filters : changes->removed_filters) {
    const std::string& app_id = removed_filters.first;
    const auto& filters = removed_filters.second;

    // To process removals for an app, go through the current list and remove
    // any filters which match the bulk update. Any items which exist in the
    // bulk update but not in the current list will be silently ignored.
    auto iter = preferred_apps_.begin();
    while (iter != preferred_apps_.end()) {
      if ((*iter)->app_id == app_id &&
          base::Contains(filters, (*iter)->intent_filter)) {
        iter = preferred_apps_.erase(iter);
      } else {
        iter++;
      }
    }

    bool has_supported_link =
        base::ranges::any_of(filters, [&app_id](const auto& filter) {
          return apps_util::IsSupportedLinkForApp(app_id, filter);
        });

    // Notify observers if any of the removed filters were supported links.
    // TODO(crbug.com/1250153): Notify observers about all changes, not just
    // changes to supported links status.
    if (has_supported_link) {
      for (auto& obs : observers_) {
        obs.OnPreferredAppChanged(app_id, false);
      }
    }
  }

  // Added filters are appended to the preferred app list with no changes.
  for (auto& added_filters : changes->added_filters) {
    const std::string& app_id = added_filters.first;
    bool has_supported_link = false;
    for (auto& filter : added_filters.second) {
      if (EntryExists(app_id, filter)) {
        continue;
      }
      has_supported_link = has_supported_link ||
                           apps_util::IsSupportedLinkForApp(app_id, filter);
      preferred_apps_.emplace_back(base::in_place, std::move(filter), app_id);
    }

    // Notify observers if any of the added filters added were supported links.
    if (has_supported_link) {
      for (auto& obs : observers_) {
        obs.OnPreferredAppChanged(app_id, true);
      }
    }
  }
}

bool PreferredAppsList::IsInitialized() const {
  return initialized_;
}

size_t PreferredAppsList::GetEntrySize() const {
  return preferred_apps_.size();
}

PreferredAppsList::PreferredApps PreferredAppsList::GetValue() const {
  PreferredAppsList::PreferredApps preferred_apps_copy;
  Clone(preferred_apps_, &preferred_apps_copy);
  return preferred_apps_copy;
}

const PreferredAppsList::PreferredApps& PreferredAppsList::GetReference()
    const {
  return preferred_apps_;
}

bool PreferredAppsList::IsPreferredAppForSupportedLinks(
    const std::string& app_id) const {
  for (const auto& preferred_app : preferred_apps_) {
    if (preferred_app->app_id == app_id &&
        apps_util::IsSupportedLinkForApp(app_id,
                                         preferred_app->intent_filter)) {
      return true;
    }
  }

  return false;
}

absl::optional<std::string> PreferredAppsList::FindPreferredAppForUrl(
    const GURL& url) const {
  auto intent = apps_util::CreateIntentFromUrl(url);
  return FindPreferredAppForIntent(intent);
}

absl::optional<std::string> PreferredAppsList::FindPreferredAppForIntent(
    const apps::mojom::IntentPtr& intent) const {
  absl::optional<std::string> best_match_app_id = absl::nullopt;
  int best_match_level = static_cast<int>(IntentFilterMatchLevel::kNone);
  for (auto& preferred_app : preferred_apps_) {
    if (apps_util::IntentMatchesFilter(intent, preferred_app->intent_filter)) {
      int match_level =
          apps_util::GetFilterMatchLevel(preferred_app->intent_filter);
      if (match_level < best_match_level) {
        continue;
      }
      best_match_level = match_level;
      best_match_app_id = preferred_app->app_id;
    }
  }
  return best_match_app_id;
}

base::flat_set<std::string> PreferredAppsList::FindPreferredAppsForFilters(
    const std::vector<apps::mojom::IntentFilterPtr>& intent_filters) const {
  base::flat_set<std::string> app_ids;

  for (auto& intent_filter : intent_filters) {
    for (auto& entry : preferred_apps_) {
      if (apps_util::FiltersHaveOverlap(intent_filter, entry->intent_filter)) {
        app_ids.insert(entry->app_id);
        break;
      }
    }
  }

  return app_ids;
}

bool PreferredAppsList::EntryExists(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  for (auto& entry : preferred_apps_) {
    if (app_id == entry->app_id && intent_filter == entry->intent_filter) {
      return true;
    }
  }
  return false;
}

}  // namespace apps
