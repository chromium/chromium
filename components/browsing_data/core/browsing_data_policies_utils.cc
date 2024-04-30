// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_policies_utils.h"

#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"

namespace browsing_data {

namespace {

namespace policy_data_types {
// Data retention policy types that require sync to be disabled.
constexpr char kBrowsingHistoryName[] = "browsing_history";
constexpr char kPasswordSigninName[] = "password_signin";
constexpr char kAutofillName[] = "autofill";
constexpr char kSiteSettingsName[] = "site_settings";
// Data retention policy types that do not require sync to be disabled.
constexpr char kHostedAppDataName[] = "hosted_app_data";
constexpr char kDownloadHistoryName[] = "download_history";
constexpr char kCookiesAndOtherSiteDataName[] = "cookies_and_other_site_data";
constexpr char kCachedImagesAndFilesName[] = "cached_images_and_files";
}  // namespace policy_data_types

// The format of the log message shown in chrome://policy/logs when sync types
// are automatically disabled.
constexpr char kDisabledSyncTypesLogFormat[] =
    "The policy %s automatically disabled the following sync types: %s";

// Adds the sync type for the `browsing_data_type` to  `sync_types` if it
// should be disabled.
void AppendSyncTypesIfRequired(const base::Value& browsing_data_type,
                               syncer::UserSelectableTypeSet* sync_types) {
  // Map of browsing data types to sync types that need to be disabled for
  // them.
  static constexpr auto kDataToSyncTypesMap =
      base::MakeFixedFlatMap<std::string_view, syncer::UserSelectableTypeSet>(
          {{policy_data_types::kBrowsingHistoryName,
            {syncer::UserSelectableType::kHistory,
             syncer::UserSelectableType::kTabs,
             syncer::UserSelectableType::kSavedTabGroups}},
           {policy_data_types::kPasswordSigninName,
            {syncer::UserSelectableType::kPasswords}},
           {policy_data_types::kSiteSettingsName,
            {syncer::UserSelectableType::kPreferences}},
           {policy_data_types::kAutofillName,
            {syncer::UserSelectableType::kAutofill,
             syncer::UserSelectableType::kPayments}},
           {policy_data_types::kDownloadHistoryName, {}},
           {policy_data_types::kCookiesAndOtherSiteDataName,
            {syncer::UserSelectableType::kCookies}},
           {policy_data_types::kCachedImagesAndFilesName, {}},
           {policy_data_types::kHostedAppDataName, {}}});

  // When a new sync type or browsing data type is introduced in the code,
  // kDataToSyncTypesMap should be updated if needed to ensure that browsing
  // data that can be cleared by policy is not already synced across devices.
  static_assert(static_cast<int>(syncer::UserSelectableType::kLastType) == 14,
                "It looks like a sync type was added or removed. Please update "
                "`kDataToSyncTypesMap` value maps above if it affects any of "
                "the browsing data types.");

  static_assert(
      static_cast<int>(PolicyDataType::kNumTypes) ==
          static_cast<int>(kDataToSyncTypesMap.size()),
      "It looks like a browsing data type that can be managed by policy was "
      "added or removed. Please update `kDataToSyncTypesMap` above to include "
      "the new type and the sync types it maps to if this data is synced.");

  const auto it = kDataToSyncTypesMap.find(browsing_data_type.GetString());
  if (it == kDataToSyncTypesMap.end()) {
    return;
  }
  for (const syncer::UserSelectableType sync_type_needed : it->second) {
    sync_types->Put(sync_type_needed);
  }
}

}  // namespace

syncer::UserSelectableTypeSet GetSyncTypesForClearBrowsingData(
    const base::Value& policy_value) {
  // The use of GetList() without type checking is safe because this
  // function is only called if the policy schema is valid.
  const auto& items = policy_value.GetList();
  syncer::UserSelectableTypeSet sync_types;
  for (const auto& item : items) {
    AppendSyncTypesIfRequired(item, &sync_types);
  }
  return sync_types;
}

syncer::UserSelectableTypeSet GetSyncTypesForBrowsingDataLifetime(
    const base::Value& policy_value) {
  // The use of GetList() and GetDict() without type checking are safe because
  // this function is only called if the policy schema is valid.
  const auto& items = policy_value.GetList();
  syncer::UserSelectableTypeSet sync_types;
  for (const auto& item : items) {
    const base::Value* data_types = item.GetDict().Find("data_types");
    for (const auto& type : data_types->GetList()) {
      AppendSyncTypesIfRequired(type, &sync_types);
    }
  }
  return sync_types;
}

std::string DisableSyncTypes(const syncer::UserSelectableTypeSet& types_set,
                             PrefValueMap* prefs,
                             const std::string& policy_name) {
  for (const syncer::UserSelectableType type : types_set) {
    syncer::SyncPrefs::SetTypeDisabledByPolicy(prefs, type);
  }
  if (types_set.size() > 0) {
    return base::StringPrintf(kDisabledSyncTypesLogFormat, policy_name.c_str(),
                              UserSelectableTypeSetToString(types_set).c_str());
  }
  return std::string();
}

std::optional<PolicyDataType> NameToPolicyDataType(
    const std::string& type_name) {
  static constexpr auto kNameToDataType =
      base::MakeFixedFlatMap<std::string_view, PolicyDataType>({
          {policy_data_types::kBrowsingHistoryName,
           PolicyDataType::kBrowsingHistory},
          {policy_data_types::kPasswordSigninName,
           PolicyDataType::kPasswordSignin},
          {policy_data_types::kAutofillName, PolicyDataType::kAutofill},
          {policy_data_types::kSiteSettingsName, PolicyDataType::kSiteSettings},
          {policy_data_types::kHostedAppDataName,
           PolicyDataType::kHostedAppData},
          {policy_data_types::kDownloadHistoryName,
           PolicyDataType::kDownloadHistory},
          {policy_data_types::kCookiesAndOtherSiteDataName,
           PolicyDataType::kCookiesAndOtherSiteData},
          {policy_data_types::kCachedImagesAndFilesName,
           PolicyDataType::kCachedImagesAndFiles},
      });

  const auto it = kNameToDataType.find(type_name);
  if (it == kNameToDataType.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace browsing_data
