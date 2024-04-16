// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_list.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "url/gurl.h"

namespace apps {

PreferredAppsList::PreferredAppsList() = default;
PreferredAppsList::~PreferredAppsList() = default;

void PreferredAppsList::Init() {
  preferred_apps_ = PreferredApps();
  initialized_ = true;
}

void PreferredAppsList::Init(PreferredApps preferred_apps) {
  preferred_apps_ = std::move(preferred_apps);
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

ReplacedAppPreferences PreferredAppsList::AddPreferredApp(
    const std::string& app_id,
    const IntentFilterPtr& intent_filter) {
  ReplacedAppPreferences replaced_app_preferences;

  if (EntryExists(app_id, intent_filter)) {
    return replaced_app_preferences;
  }

  auto iter = preferred_apps_.begin();

  // Go through the list and see if there are overlapped intent filters in the
  // list. If there is, add this into the replaced_app_preferences and remove it
  // from the list.
  while (iter != preferred_apps_.end()) {
    // Only replace overlapped intent filters for other apps.
    if ((*iter)->app_id != app_id &&
        apps_util::FiltersHaveOverlap((*iter)->intent_filter, intent_filter)) {
      // Add the to be removed preferred app into a map, key by app_id.
      replaced_app_preferences[(*iter)->app_id].push_back(
          std::move((*iter)->intent_filter));
      iter = preferred_apps_.erase(iter);
    } else {
      iter++;
    }
  }
  preferred_apps_.push_back(
      std::make_unique<PreferredApp>(intent_filter->Clone(), app_id));

  if (apps_util::IsSupportedLinkForApp(app_id, intent_filter)) {
    for (auto& obs : observers_) {
      obs.OnPreferredAppChanged(app_id, true);
      for (auto& app : replaced_app_preferences) {
        obs.OnPreferredAppChanged(app.first, false);
      }
    }
  }
  return replaced_app_preferences;
}

IntentFilters PreferredAppsList::DeletePreferredApp(
    const std::string& app_id,
    const IntentFilterPtr& intent_filter) {
  // Go through the list and see if there are overlapped intent filters with the
  // same app id in the list. If there are, delete the entry.
  IntentFilters out;
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

IntentFilters PreferredAppsList::DeleteAppId(const std::string& app_id) {
  IntentFilters out;

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

IntentFilters PreferredAppsList::DeleteSupportedLinks(
    const std::string& app_id) {
  IntentFilters out;

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

void PreferredAppsList::ApplyBulkUpdate(apps::PreferredAppChangesPtr changes) {
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
          Contains(filters, (*iter)->intent_filter)) {
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
    // TODO(crbug.com/40791690): Notify observers about all changes, not just
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
      preferred_apps_.push_back(
          std::make_unique<PreferredApp>(std::move(filter), app_id));
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

PreferredApps PreferredAppsList::GetValue() const {
  return ClonePreferredApps(preferred_apps_);
}

const PreferredApps& PreferredAppsList::GetReference() const {
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

std::optional<std::string> PreferredAppsList::FindPreferredAppForUrl(
    const GURL& url) const {
  return FindPreferredAppForIntent(
      std::make_unique<Intent>(apps_util::kIntentActionView, url));
}

std::optional<std::string> PreferredAppsList::FindPreferredAppForIntent(
    const IntentPtr& intent) const {
  std::optional<std::string> best_match_app_id = std::nullopt;
  int best_match_level = static_cast<int>(IntentFilterMatchLevel::kNone);
  DCHECK(intent);
  for (auto& preferred_app : preferred_apps_) {
    if (intent->MatchFilter(preferred_app->intent_filter)) {
      int match_level = preferred_app->intent_filter->GetFilterMatchLevel();
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
    const IntentFilters& intent_filters) const {
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

bool PreferredAppsList::EntryExists(const std::string& app_id,
                                    const IntentFilterPtr& intent_filter) {
  for (auto& entry : preferred_apps_) {
    if (app_id == entry->app_id && *intent_filter == *entry->intent_filter) {
      return true;
    }
  }
  return false;
}

}  // namespace apps
