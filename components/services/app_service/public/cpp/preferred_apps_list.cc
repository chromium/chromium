// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_apps_list.h"

#include <algorithm>
#include <utility>

#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "url/gurl.h"

namespace apps {

PreferredAppsList::PreferredAppsList(Delegate* delegate)
    : delegate_(delegate) {}
PreferredAppsList::~PreferredAppsList() = default;

void PreferredAppsList::Init() {
  preferred_apps_ = PreferredApps();
  initialized_ = true;
  for (auto& obs : observers_) {
    obs.OnPreferredAppsListInitialized();
  }
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
  for (auto& obs : observers_) {
    obs.OnPreferredAppsListInitialized();
  }
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
      bool has_conflict = true;
      if (delegate_) {
        has_conflict = delegate_->QueryConflict(
            (*iter)->app_id, (*iter)->intent_filter, app_id, intent_filter);
      }
      if (has_conflict) {
        // Add the to be removed preferred app into a map, key by app_id.
        replaced_app_preferences[(*iter)->app_id].push_back(
            std::move((*iter)->intent_filter));
        iter = preferred_apps_.erase(iter);
      } else {
        iter++;
      }
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
  size_t best_match_length = 0;
  DCHECK(intent);
  std::vector<const PreferredApp*> web_apps_that_match_with_scope_extensions;

  for (auto& preferred_app : preferred_apps_) {
    if (intent->MatchFilter(preferred_app->intent_filter)) {
      // If it is a scope extension, record it for fallback and continue.
      if (delegate_ && intent->url.has_value() &&
          delegate_->IsWebAppInExtendedScope(*intent->url,
                                             preferred_app->app_id)) {
        web_apps_that_match_with_scope_extensions.push_back(
            preferred_app.get());
        continue;
      }
      int match_level = preferred_app->intent_filter->GetFilterMatchLevel();
      if (match_level < best_match_level) {
        continue;
      }
      size_t match_length = 0;
      if (intent->url.has_value()) {
        match_length = apps_util::IntentFilterUrlMatchLength(
            preferred_app->intent_filter, *intent->url);
      }
      // If the match level is identical to the current best match level, break
      // the tie by using the URL prefix match length (longer URL scope wins).
      if (longest_prefix_match_enabled_ && match_level == best_match_level) {
        if (match_length < best_match_length) {
          continue;
        }
      }
      best_match_level = match_level;
      best_match_length = match_length;
      best_match_app_id = preferred_app->app_id;
    }
  }
  if (best_match_app_id.has_value()) {
    return best_match_app_id;
  }

  // If code reached here, it is guaranteed that there are no apps that is the
  // preferred app for capturing links. However, there can still be a web app
  // that is the preferred app for capturing links based on the scope extensions
  // stored in it. In that case, return that web app.
  best_match_level = static_cast<int>(IntentFilterMatchLevel::kNone);
  best_match_length = 0;

  for (const auto* candidate : web_apps_that_match_with_scope_extensions) {
    int match_level = candidate->intent_filter->GetFilterMatchLevel();
    if (match_level < best_match_level) {
      continue;
    }
    size_t match_length = 0;
    if (intent->url.has_value()) {
      match_length = apps_util::IntentFilterUrlMatchLength(
          candidate->intent_filter, *intent->url);
    }
    if (longest_prefix_match_enabled_ && match_level == best_match_level) {
      if (match_length < best_match_length) {
        continue;
      }
    }
    best_match_level = match_level;
    best_match_length = match_length;
    best_match_app_id = candidate->app_id;
  }

  return best_match_app_id;
}

base::flat_set<std::string> PreferredAppsList::FindPreferredAppsForFilters(
    std::optional<std::string> app_id,
    const IntentFilters& intent_filters) const {
  base::flat_set<std::string> app_ids;

  for (auto& intent_filter : intent_filters) {
    for (auto& entry : preferred_apps_) {
      // Check if another app has a preferred filter that structurally
      // overlaps with this filter. If so, query the conflict callback to
      // determine if they actually conflict (cannot co-exist).
      if ((!app_id.has_value() || entry->app_id != *app_id) &&
          apps_util::FiltersHaveOverlap(intent_filter, entry->intent_filter)) {
        bool has_conflict = true;
        if (app_id.has_value() && delegate_) {
          has_conflict = delegate_->QueryConflict(
              entry->app_id, entry->intent_filter, *app_id, intent_filter);
        }
        // If there is a conflict, we add the conflicting app to the set of
        // apps to disable before enabling the new app.
        if (has_conflict) {
          app_ids.insert(entry->app_id);
        }
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
