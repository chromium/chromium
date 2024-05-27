// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_policies_utils.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Checks that the sync types list is updated correctly given a
// BrowsingDataLifetimePolicy value.
TEST(BrowsingDataPoliciesUtils, UpdateSyncTypesForBrowsingDataLifetime) {
  base::Value::Dict browsing_data_types_first_dict =
      base::Value::Dict()
          .Set("data_types", base::Value::List()
                                 .Append("browsing_history")
                                 .Append("site_settings")
                                 .Append("cached_images_and_files")
                                 .Append("cookies_and_other_site_data"))
          .Set("time_to_live_in_hours", 1);

  base::Value::Dict browsing_data_types_second_dict =
      base::Value::Dict()
          .Set("data_types", base::Value::List()
                                 .Append("autofill")
                                 .Append("password_signin")
                                 .Append("hosted_app_data")
                                 .Append("download_history"))
          .Set("time_to_live_in_hours", 1);

  base::Value browsing_data_lifetime_value =
      base::Value(base::Value::List()
                      .Append(std::move(browsing_data_types_first_dict))
                      .Append(std::move(browsing_data_types_second_dict)));

  // A total of 7 sync types needed for browsing_history, autofill,
  // passwords_signin, site settings and cookies will be added. No sync type
  // will be added for the other types.
  syncer::UserSelectableTypeSet sync_types =
      browsing_data::GetSyncTypesForBrowsingDataLifetime(
          browsing_data_lifetime_value);
  const syncer::UserSelectableTypeSet expected_types = {
      syncer::UserSelectableType::kAutofill,
      syncer::UserSelectableType::kPayments,
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kHistory,
      syncer::UserSelectableType::kTabs,
      syncer::UserSelectableType::kSavedTabGroups,
      syncer::UserSelectableType::kCookies};
  EXPECT_EQ(sync_types, expected_types);
}

// Checks that the sync types list is updated correctly given a
// ClearBrowsingDataOnExit value.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
TEST(BrowsingDataPoliciesUtils, UpdateSyncTypesForClearBrowsingDataOnExit) {
  base::Value::List clear_browsing_data_list =
      base::Value::List()
          .Append("autofill")
          .Append("password_signin")
          .Append("browsing_history")
          .Append("site_settings")
          .Append("cached_images_and_files")
          .Append("cookies_and_other_site_data")
          .Append("hosted_app_data")
          .Append("download_history");

  base::Value clear_browsing_data_on_exit_value =
      base::Value(std::move(clear_browsing_data_list));

  // A total of 7 sync types needed for browsing_history, autofill,
  // passwords_signin, site settings and cookies will be added. No sync type
  // will be added for the other types.
  syncer::UserSelectableTypeSet sync_types =
      browsing_data::GetSyncTypesForClearBrowsingData(
          clear_browsing_data_on_exit_value);
  const syncer::UserSelectableTypeSet expected_types = {
      syncer::UserSelectableType::kAutofill,
      syncer::UserSelectableType::kPayments,
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kHistory,
      syncer::UserSelectableType::kTabs,
      syncer::UserSelectableType::kSavedTabGroups,
      syncer::UserSelectableType::kCookies};
  EXPECT_EQ(sync_types, expected_types);
}
#endif

TEST(BrowsingDataPoliciesUtils, NameToPolicyDataType) {
  EXPECT_EQ(browsing_data::NameToPolicyDataType("browsing_history"),
            browsing_data::PolicyDataType::kBrowsingHistory);
  EXPECT_EQ(browsing_data::NameToPolicyDataType("password_signin"),
            browsing_data::PolicyDataType::kPasswordSignin);
  EXPECT_EQ(browsing_data::NameToPolicyDataType("autofill"),
            browsing_data::PolicyDataType::kAutofill);
  EXPECT_EQ(browsing_data::NameToPolicyDataType("site_settings"),
            browsing_data::PolicyDataType::kSiteSettings);
  EXPECT_EQ(browsing_data::NameToPolicyDataType("hosted_app_data"),
            browsing_data::PolicyDataType::kHostedAppData);
  EXPECT_EQ(browsing_data::NameToPolicyDataType("download_history"),
            browsing_data::PolicyDataType::kDownloadHistory);
  EXPECT_EQ(browsing_data::NameToPolicyDataType("cookies_and_other_site_data"),
            browsing_data::PolicyDataType::kCookiesAndOtherSiteData);
  EXPECT_EQ(browsing_data::NameToPolicyDataType("cached_images_and_files"),
            browsing_data::PolicyDataType::kCachedImagesAndFiles);
}

// This test checks that all sync types currently available in Chrome are known
// and properly handled.
TEST(BrowsingDataPoliciesUtils, AllSyncTypesChecked) {
  // Set policy value to all browsing data types to disable all sync types that
  // might be disabled for the policy.
  base::Value::List clear_browsing_data_list =
      base::Value::List()
          .Append("autofill")
          .Append("password_signin")
          .Append("browsing_history")
          .Append("site_settings")
          .Append("cached_images_and_files")
          .Append("cookies_and_other_site_data")
          .Append("hosted_app_data")
          .Append("download_history");

  base::Value clear_browsing_data_on_exit_value(
      std::move(clear_browsing_data_list));

  // The sync types that are known to never be disabled as a result of setting
  // the policy.
  syncer::UserSelectableTypeSet always_enabled_sync_types = {
      syncer::UserSelectableType::kBookmarks,
      syncer::UserSelectableType::kProductComparison,
      syncer::UserSelectableType::kThemes,
      syncer::UserSelectableType::kExtensions,
      syncer::UserSelectableType::kApps,
      syncer::UserSelectableType::kReadingList,
      syncer::UserSelectableType::kSharedTabGroupData};

  syncer::UserSelectableTypeSet sync_types =
      browsing_data::GetSyncTypesForClearBrowsingData(
          clear_browsing_data_on_exit_value);

  // Every sync type should be mapped to a browsing-data type in
  // `kDataToSyncTypesMap` in browsing_data_policies_utils.cc. If a sync type is
  // not affected by any browsing-data type, it can be added to
  // `always_enabled_sync_types` in this test.
  for (const syncer::UserSelectableType sync_type :
       syncer::UserSelectableTypeSet::All()) {
    EXPECT_TRUE(sync_types.Has(sync_type) ||
                always_enabled_sync_types.Has(sync_type));
  }
}
