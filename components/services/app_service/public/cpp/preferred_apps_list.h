// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"

class GURL;

namespace apps {

// The preferred apps set by the user. The preferred apps is stored as
// an list of |intent_filter| vs. app_id.
class PreferredAppsList : public PreferredAppsListHandle {
 public:
  PreferredAppsList();
  ~PreferredAppsList();

  PreferredAppsList(const PreferredAppsList&) = delete;
  PreferredAppsList& operator=(const PreferredAppsList&) = delete;

  // Initialize the preferred app with empty list or existing |preferred_apps|;
  void Init();
  void Init(PreferredApps preferred_apps);

  // Add a preferred app for an |intent_filter|, and returns a group of
  // |app_ids| that is no longer preferred app of their corresponding
  // |intent_filters|.
  ReplacedAppPreferences AddPreferredApp(const std::string& app_id,
                                         const IntentFilterPtr& intent_filter);

  // Delete a preferred app for an |intent_filter| with the same |app_id|.
  // Returns the deleted filters, if any.
  IntentFilters DeletePreferredApp(const std::string& app_id,
                                   const IntentFilterPtr& intent_filter);

  // Delete all settings for an |app_id|. Returns the deleted filters, if any.
  IntentFilters DeleteAppId(const std::string& app_id);

  // Deletes all stored supported link preferences for an |app_id|.
  // Returns the deleted filters, if any.
  IntentFilters DeleteSupportedLinks(const std::string& app_id);

  // Applies all of the |changes| in a single bulk update. This method is
  // intended to only be called from |OnPreferredAppsChanged| App Service
  // subscriber overrides.
  // Note that removed filters are processed before new filters are added. If
  // the same filter appears in both |changes->added_filters| and
  // |changes->removed_filters|, it be removed and then immediately added back.
  void ApplyBulkUpdate(apps::PreferredAppChangesPtr changes);

  // PreferredAppsListHandler overrides:
  bool IsInitialized() const override;
  size_t GetEntrySize() const override;
  PreferredApps GetValue() const override;
  const PreferredApps& GetReference() const override;
  bool IsPreferredAppForSupportedLinks(
      const std::string& app_id) const override;
  std::optional<std::string> FindPreferredAppForUrl(
      const GURL& url) const override;
  std::optional<std::string> FindPreferredAppForIntent(
      const IntentPtr& intent) const override;
  base::flat_set<std::string> FindPreferredAppsForFilters(
      const IntentFilters& intent_filters) const override;

 private:
  // Check if the entry already exists in the preferred app list.
  bool EntryExists(const std::string& app_id,
                   const IntentFilterPtr& intent_filter);

  PreferredApps preferred_apps_;
  bool initialized_ = false;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
