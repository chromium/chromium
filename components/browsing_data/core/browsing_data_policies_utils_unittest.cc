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

  // A total of 6 sync types needed for browsing_history, autofill,
  // passwords_signin and site settings will be added. No sync type will be
  // added for the other types.
  syncer::UserSelectableTypeSet sync_types =
      browsing_data::GetSyncTypesForBrowsingDataLifetime(
          browsing_data_lifetime_value);
  syncer::UserSelectableTypeSet expected_types = syncer::UserSelectableTypeSet(
      syncer::UserSelectableType::kAutofill,
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kHistory, syncer::UserSelectableType::kTabs,
      syncer::UserSelectableType::kSavedTabGroups);
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

  // A total of 6 sync types needed for browsing_history, autofill,
  // passwords_signin and site settings will be added. No sync type will be
  // added for the other types.
  syncer::UserSelectableTypeSet sync_types =
      browsing_data::GetSyncTypesForClearBrowsingData(
          clear_browsing_data_on_exit_value);
  syncer::UserSelectableTypeSet expected_types = syncer::UserSelectableTypeSet(
      syncer::UserSelectableType::kAutofill,
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kHistory, syncer::UserSelectableType::kTabs,
      syncer::UserSelectableType::kSavedTabGroups);
  EXPECT_EQ(sync_types, expected_types);
}
#endif
