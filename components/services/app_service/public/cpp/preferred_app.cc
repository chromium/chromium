// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/preferred_app.h"

namespace apps {

PreferredApp::PreferredApp(IntentFilterPtr intent_filter,
                           const std::string& app_id)
    : intent_filter(std::move(intent_filter)), app_id(app_id) {}

PreferredApp::~PreferredApp() = default;

bool PreferredApp::operator==(const PreferredApp& other) const {
  return *intent_filter == *other.intent_filter && app_id == other.app_id;
}

bool PreferredApp::operator!=(const PreferredApp& other) const {
  return !(*this == other);
}

std::unique_ptr<PreferredApp> PreferredApp::Clone() const {
  return std::make_unique<PreferredApp>(intent_filter->Clone(), app_id);
}

PreferredAppChanges::PreferredAppChanges() = default;

PreferredAppChanges::~PreferredAppChanges() = default;

PreferredAppChangesPtr PreferredAppChanges::Clone() const {
  auto preferred_app_changes = std::make_unique<PreferredAppChanges>();
  for (const auto& added_filter : added_filters) {
    apps::IntentFilters filters;
    for (auto& filter : added_filter.second) {
      filters.push_back(filter->Clone());
    }
    preferred_app_changes->added_filters[added_filter.first] =
        std::move(filters);
  }
  for (const auto& removed_filter : removed_filters) {
    apps::IntentFilters filters;
    for (auto& filter : removed_filter.second) {
      filters.push_back(filter->Clone());
    }
    preferred_app_changes->removed_filters[removed_filter.first] =
        std::move(filters);
  }
  return preferred_app_changes;
}

PreferredApps ClonePreferredApps(const PreferredApps& preferred_apps) {
  PreferredApps ret;
  ret.reserve(preferred_apps.size());
  for (const auto& preferred_app : preferred_apps) {
    ret.push_back(preferred_app->Clone());
  }
  return ret;
}

bool IsEqual(const PreferredApps& source, const PreferredApps& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }
  return true;
}

PreferredAppPtr ConvertMojomPreferredAppToPreferredApp(
    const apps::mojom::PreferredAppPtr& mojom_preferred_app) {
  if (!mojom_preferred_app) {
    return nullptr;
  }

  PreferredAppPtr preferred_app =
      std::make_unique<PreferredApp>(ConvertMojomIntentFilterToIntentFilter(
                                         mojom_preferred_app->intent_filter),
                                     mojom_preferred_app->app_id);
  return preferred_app;
}

apps::mojom::PreferredAppPtr ConvertPreferredAppToMojomPreferredApp(
    const PreferredAppPtr& preferred_app) {
  auto mojom_preferred_app = apps::mojom::PreferredApp::New();
  if (!preferred_app) {
    return mojom_preferred_app;
  }

  mojom_preferred_app->intent_filter =
      ConvertIntentFilterToMojomIntentFilter(preferred_app->intent_filter);
  mojom_preferred_app->app_id = preferred_app->app_id;
  return mojom_preferred_app;
}

PreferredAppChangesPtr ConvertMojomPreferredAppChangesToPreferredAppChanges(
    const apps::mojom::PreferredAppChangesPtr& mojom_preferred_app_changes) {
  if (!mojom_preferred_app_changes) {
    return nullptr;
  }

  PreferredAppChangesPtr preferred_app_changes =
      std::make_unique<PreferredAppChanges>();
  for (const auto& added_filters : mojom_preferred_app_changes->added_filters) {
    apps::IntentFilters filters;
    for (auto& filter : added_filters.second) {
      filters.push_back(ConvertMojomIntentFilterToIntentFilter(filter));
    }
    preferred_app_changes->added_filters[added_filters.first] =
        std::move(filters);
  }
  for (const auto& removed_filters :
       mojom_preferred_app_changes->removed_filters) {
    apps::IntentFilters filters;
    for (auto& filter : removed_filters.second) {
      filters.push_back(ConvertMojomIntentFilterToIntentFilter(filter));
    }
    preferred_app_changes->removed_filters[removed_filters.first] =
        std::move(filters);
  }
  return preferred_app_changes;
}

apps::mojom::PreferredAppChangesPtr
ConvertPreferredAppChangesToMojomPreferredAppChanges(
    const PreferredAppChangesPtr& preferred_app_changes) {
  auto mojom_preferred_app_changes = apps::mojom::PreferredAppChanges::New();
  if (!preferred_app_changes) {
    return mojom_preferred_app_changes;
  }

  for (const auto& added_filters : preferred_app_changes->added_filters) {
    std::vector<apps::mojom::IntentFilterPtr> mojom_filters;
    for (auto& filter : added_filters.second) {
      mojom_filters.push_back(ConvertIntentFilterToMojomIntentFilter(filter));
    }
    mojom_preferred_app_changes->added_filters[added_filters.first] =
        std::move(mojom_filters);
  }
  for (const auto& removed_filters : preferred_app_changes->removed_filters) {
    std::vector<apps::mojom::IntentFilterPtr> mojom_filters;
    for (auto& filter : removed_filters.second) {
      mojom_filters.push_back(ConvertIntentFilterToMojomIntentFilter(filter));
    }
    mojom_preferred_app_changes->removed_filters[removed_filters.first] =
        std::move(mojom_filters);
  }
  return mojom_preferred_app_changes;
}

PreferredApps ConvertMojomPreferredAppsToPreferredApps(
    const std::vector<apps::mojom::PreferredAppPtr>& mojom_preferred_apps) {
  PreferredApps ret;
  ret.reserve(mojom_preferred_apps.size());
  for (const auto& mojom_preferred_app : mojom_preferred_apps) {
    ret.push_back(ConvertMojomPreferredAppToPreferredApp(mojom_preferred_app));
  }
  return ret;
}

std::vector<apps::mojom::PreferredAppPtr>
ConvertPreferredAppsToMojomPreferredApps(const PreferredApps& preferred_apps) {
  std::vector<apps::mojom::PreferredAppPtr> ret;
  ret.reserve(preferred_apps.size());
  for (const auto& preferred_app : preferred_apps) {
    ret.push_back(ConvertPreferredAppToMojomPreferredApp(preferred_app));
  }
  return ret;
}

apps::mojom::ReplacedAppPreferencesPtr
ConvertReplacedAppPreferencesToMojomReplacedAppPreferences(
    const ReplacedAppPreferences& replace_preferences) {
  auto replaced_app_preferences = apps::mojom::ReplacedAppPreferences::New();
  auto& replaced_preference_map = replaced_app_preferences->replaced_preference;
  for (const auto& it : replace_preferences) {
    for (const auto& filter : it.second) {
      replaced_preference_map[it.first].push_back(
          ConvertIntentFilterToMojomIntentFilter(filter));
    }
  }
  return replaced_app_preferences;
}

ReplacedAppPreferences
ConvertMojomReplacedAppPreferencesToReplacedAppPreferences(
    const apps::mojom::ReplacedAppPreferencesPtr& mojom_replace_preferences) {
  ReplacedAppPreferences replaced_app_preferences;
  if (!mojom_replace_preferences) {
    return replaced_app_preferences;
  }
  auto& mojom_replaced_preference_map =
      mojom_replace_preferences->replaced_preference;
  for (const auto& it : mojom_replaced_preference_map) {
    for (const auto& filter : it.second) {
      replaced_app_preferences[it.first].push_back(
          ConvertMojomIntentFilterToIntentFilter(filter));
    }
  }
  return replaced_app_preferences;
}

}  // namespace apps
