// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_POLICIES_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_POLICIES_UTILS_H_

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/user_selectable_type.h"

namespace browsing_data {

// The data types values of the BrowsingDataSettings policy.
namespace policy_data_types {
extern const char kBrowsingHistory[];
extern const char kDownloadHistory[];
extern const char kCookiesAndOtherSiteData[];
extern const char kCachedImagesAndFiles[];
extern const char kPasswordSignin[];
extern const char kAutofill[];
extern const char kSiteSettings[];
extern const char kHostedAppData[];
}  // namespace policy_data_types

// If the BrowsingDataLifetime list of dictionaries have data types that are
// synced, the corresponding sync types will be added to `sync_types`.
syncer::UserSelectableTypeSet GetSyncTypesForBrowsingDataLifetime(
    const base::Value& policy_value);

// If the ClearBrowsingDataOn* list has data types that are synced, the
// corresponding sync types will be added to `sync_types`.
syncer::UserSelectableTypeSet GetSyncTypesForClearBrowsingData(
    const base::Value& policy_value);

// Disables all the sync types in `types_set` by disabling their preferences and
// adds a log for the chrome://policy/logs page.
void DisableSyncTypes(const syncer::UserSelectableTypeSet& types_set,
                      PrefValueMap* prefs,
                      const std::string& policy_name,
                      std::string& log_message);

// Check if data retention policies dependency on sync types is enabled by
// feature.
bool IsPolicyDependencyEnabled();

}  // namespace browsing_data
#endif  // COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_POLICIES_UTILS_H_
