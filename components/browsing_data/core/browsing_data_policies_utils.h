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

// The data types of the BrowsingDataSettings policy.
enum class PolicyDataType {
  kBrowsingHistory = 0,
  kPasswordSignin = 1,
  kAutofill = 2,
  kSiteSettings = 3,
  kHostedAppData = 4,
  kDownloadHistory = 5,
  kCookiesAndOtherSiteData = 6,
  kCachedImagesAndFiles = 7,
  kNumTypes = kCachedImagesAndFiles + 1
};

// If the BrowsingDataLifetime list of dictionaries have data types that are
// synced, the corresponding sync types will be added to `sync_types`.
syncer::UserSelectableTypeSet GetSyncTypesForBrowsingDataLifetime(
    const base::Value& policy_value);

// If the ClearBrowsingDataOn* list has data types that are synced, the
// corresponding sync types will be added to `sync_types`.
syncer::UserSelectableTypeSet GetSyncTypesForClearBrowsingData(
    const base::Value& policy_value);

// Disables all the sync types in `types_set` by disabling their preferences.
// Returns a message with the disabled sync types. The message is suitable for
// logging to chrome://policy/logs.
std::string DisableSyncTypes(const syncer::UserSelectableTypeSet& types_set,
                             PrefValueMap* prefs,
                             const std::string& policy_name);

// Converts the browsing data type string to its integer value.
// The conversion is used to ensure that the number of policy data types that
// can be managed at any point in time is known and mapped to sync types that
// need to be disabled if it is managed by policy.
std::optional<PolicyDataType> NameToPolicyDataType(const std::string& type);

}  // namespace browsing_data
#endif  // COMPONENTS_BROWSING_DATA_CORE_BROWSING_DATA_POLICIES_UTILS_H_
