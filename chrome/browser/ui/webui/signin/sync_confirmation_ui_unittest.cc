// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"

#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;
using testing::HasSubstr;
using testing::Not;
using testing::ValuesIn;

struct TestParam {
  // Syncable data types to disable, simulating effects of policies.
  const UserSelectableTypeSet disabled_types;

  // Strings expected to be found in the sync benefits JSON data.
  const std::vector<std::string> expected_benefit_substr;

  // Strings NOT expected to be found in the sync benefits JSON data.
  const std::vector<std::string> unexpected_benefit_keys;
};

UserSelectableTypeSet AllUserSelectableTypesExcept(
    UserSelectableTypeSet excluded_types) {
  UserSelectableTypeSet selection = UserSelectableTypeSet::All();
  selection.RemoveAll(excluded_types);
  return selection;
}

const TestParam kGetSyncBenefitsListJSONParams[] = {
    // 0: Default.
    {.disabled_types = {},
     .expected_benefit_substr = {"syncConfirmationAutofill",
                                 "syncConfirmationBookmarks",
                                 "syncConfirmationExtensions",
                                 "syncConfirmationHistoryAndMore"},
     .unexpected_benefit_keys = {"syncConfirmationReadingList"}},

    // 1: Disabling certain types does not affect the benefits list.
    {.disabled_types = AllUserSelectableTypesExcept(
         {UserSelectableType::kBookmarks, UserSelectableType::kApps,
          UserSelectableType::kPasswords}),
     .expected_benefit_substr = {"syncConfirmationAutofill",
                                 "syncConfirmationBookmarks",
                                 "syncConfirmationExtensions",
                                 "syncConfirmationHistoryAndMore"},
     .unexpected_benefit_keys = {"syncConfirmationReadingList"}},

    // 2: "Reading List" instead of "Bookmarks", no "Autofill" nor "Extensions"
    {.disabled_types = {UserSelectableType::kBookmarks,
                        UserSelectableType::kAutofill,
                        UserSelectableType::kPasswords,
                        UserSelectableType::kApps,
                        UserSelectableType::kExtensions},
     .expected_benefit_substr = {"syncConfirmationReadingList",
                                 "syncConfirmationHistoryAndMore"},
     .unexpected_benefit_keys = {"syncConfirmationBookmarks",
                                 "syncConfirmationAutofill",
                                 "syncConfirmationExtensions"}},
};

void DisableTypes(sync_preferences::TestingPrefServiceSyncable& pref_service,
                  syncer::UserSelectableTypeSet types) {
  for (auto type : types) {
    std::string path = syncer::SyncPrefs::GetPrefNameForType(type);
    pref_service.SetManagedPref(path, base::Value(false));
  }
}

class SyncConfirmationUITest : public testing::TestWithParam<TestParam> {};

TEST_P(SyncConfirmationUITest, GetSyncBenefitsListJSON) {
  auto pref_service =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  syncer::SyncPrefs::RegisterProfilePrefs(pref_service->registry());

  DisableTypes(*pref_service.get(), GetParam().disabled_types);

  std::string benefits_json =
      SyncConfirmationUI::GetSyncBenefitsListJSON(*pref_service.get());
  for (auto expected_key : GetParam().expected_benefit_substr) {
    EXPECT_THAT(benefits_json, HasSubstr(expected_key));
  }
  for (auto unexpected_key : GetParam().unexpected_benefit_keys) {
    EXPECT_THAT(benefits_json, Not(HasSubstr(unexpected_key)));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUITest,
                         ValuesIn(kGetSyncBenefitsListJSONParams));
