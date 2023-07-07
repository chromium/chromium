// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APP_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APP_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "components/services/app_service/public/cpp/intent_filter.h"

namespace apps {

// The preferred app represents by `app_id` for `intent_filter`.
struct COMPONENT_EXPORT(APP_TYPES) PreferredApp {
  PreferredApp(IntentFilterPtr intent_filter, const std::string& app_id);
  PreferredApp(const PreferredApp&) = delete;
  PreferredApp& operator=(const PreferredApp&) = delete;
  ~PreferredApp();

  bool operator==(const PreferredApp& other) const;
  bool operator!=(const PreferredApp& other) const;

  std::unique_ptr<PreferredApp> Clone() const;

  IntentFilterPtr intent_filter;
  std::string app_id;
};

using PreferredAppPtr = std::unique_ptr<PreferredApp>;
using PreferredApps = std::vector<PreferredAppPtr>;

// Represents changes which have been made to the preferred apps list, both
// adding new filters and removing existing filters.
struct COMPONENT_EXPORT(APP_TYPES) PreferredAppChanges {
  PreferredAppChanges();
  PreferredAppChanges(const PreferredAppChanges&) = delete;
  PreferredAppChanges& operator=(const PreferredAppChanges&) = delete;
  ~PreferredAppChanges();

  std::unique_ptr<PreferredAppChanges> Clone() const;

  base::flat_map<std::string, IntentFilters> added_filters;
  base::flat_map<std::string, IntentFilters> removed_filters;
};

using PreferredAppChangesPtr = std::unique_ptr<PreferredAppChanges>;

// Represents a group of `app_ids` that is no longer preferred app of their
// corresponding `intent_filters`.
using ReplacedAppPreferences = base::flat_map<std::string, IntentFilters>;

// Creates a deep copy of `preferred_apps`.
COMPONENT_EXPORT(APP_TYPES)
PreferredApps ClonePreferredApps(const PreferredApps& preferred_apps);

COMPONENT_EXPORT(APP_TYPES)
bool IsEqual(const PreferredApps& source, const PreferredApps& target);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APP_H_
