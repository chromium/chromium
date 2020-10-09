// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class GURL;

namespace apps {

// The preferred apps set by the user. The preferred apps is stored as
// an list of |intent_filter| vs. app_id.
class PreferredAppsList {
 public:
  PreferredAppsList();
  ~PreferredAppsList();

  PreferredAppsList(const PreferredAppsList&) = delete;
  PreferredAppsList& operator=(const PreferredAppsList&) = delete;

  using PreferredApps = std::vector<apps::mojom::PreferredAppPtr>;

  // Find preferred app id for an |intent|.
  base::Optional<std::string> FindPreferredAppForIntent(
      const apps::mojom::IntentPtr& intent);

  // Find preferred app id for an |url|.
  base::Optional<std::string> FindPreferredAppForUrl(const GURL& url);

  // Add a preferred app for an |intent_filter|, and returns a group of
  // |app_ids| that is no longer preferred app of their corresponding
  // |intent_filters|.
  apps::mojom::ReplacedAppPreferencesPtr AddPreferredApp(
      const std::string& app_id,
      const apps::mojom::IntentFilterPtr& intent_filter);

  // Delete a preferred app for an |intent_filter| with the same |app_id|.
  void DeletePreferredApp(const std::string& app_id,
                          const apps::mojom::IntentFilterPtr& intent_filter);

  // Delete all settings for an |app_id|.
  void DeleteAppId(const std::string& app_id);

  // Initialize the preferred app with empty list or existing |preferred_apps|;
  void Init();
  void Init(PreferredApps& preferred_apps);

  // Get a copy of the preferred apps.
  PreferredApps GetValue();

  bool IsInitialized();

  const PreferredApps& GetReference() const;

  // Get the entry size of the preferred app list.
  size_t GetEntrySize();

 private:
  PreferredApps preferred_apps_;
  bool initialized_ = false;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
