// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_preferences.h"

#include <memory>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_sync_data_fake.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

class SupervisedUserPreferencesTest : public ::testing::Test {
 public:
  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());
    supervised_user_sync_data_fake_.Init();
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  test::SupervisedUserSyncDataFake<TestingPrefServiceSimple>
      supervised_user_sync_data_fake_{pref_service_};
};

TEST_F(SupervisedUserPreferencesTest, RegisterProfilePrefsAndCheckDefaults) {
  // Checks the preference registration from the Setup.
  EXPECT_EQ(
      pref_service_.GetInteger(prefs::kDefaultSupervisedUserFilteringBehavior),
      static_cast<int>(FilteringBehavior::kAllow));

  // This is browser's default kSupervisedUserSafeSites setting (for
  // unsupervised user).
  EXPECT_EQ(pref_service_.GetBoolean(prefs::kSupervisedUserSafeSites), false);
  EXPECT_FALSE(IsSubjectToParentalControls(pref_service_));
  // TODO(b/306376651): When we migrate more preference reading methods in this
  // library, add more test cases for their correct default values.
}

TEST_F(SupervisedUserPreferencesTest, ToggleParentalControlsSetsUserId) {
  EnableParentalControls(pref_service_);
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserId),
            kChildAccountSUID);

  DisableParentalControls(pref_service_);
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserId), std::string());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SupervisedUserPreferencesTest, ToggleParentalControlsSetsChildStatus) {
  EnableParentalControls(pref_service_);
  EXPECT_TRUE(IsChildAccountStatusKnown(pref_service_));

  DisableParentalControls(pref_service_);
  EXPECT_TRUE(IsChildAccountStatusKnown(pref_service_));
}
#endif

TEST_F(SupervisedUserPreferencesTest, StartFetchingFamilyInfo) {
  kidsmanagement::ListMembersResponse list_family_members_response;
  SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(),
      kidsmanagement::HEAD_OF_HOUSEHOLD, "username_hoh");
  SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(), kidsmanagement::PARENT,
      "username_parent");
  SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(), kidsmanagement::CHILD,
      "username_child");
  SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(), kidsmanagement::MEMBER,
      "username_member");

  RegisterFamilyPrefs(pref_service_, list_family_members_response);

  EXPECT_EQ("username_hoh",
            pref_service_.GetString(prefs::kSupervisedUserCustodianName));
  EXPECT_EQ("username_parent",
            pref_service_.GetString(prefs::kSupervisedUserSecondCustodianName));
}

TEST_F(SupervisedUserPreferencesTest, FieldsAreClearedForNonChildAccounts) {
  {
    kidsmanagement::ListMembersResponse list_family_members_response;
    SetFamilyMemberAttributesForTesting(
        list_family_members_response.add_members(),
        kidsmanagement::HEAD_OF_HOUSEHOLD, "username_hoh");
    SetFamilyMemberAttributesForTesting(
        list_family_members_response.add_members(), kidsmanagement::PARENT,
        "username_parent");

    RegisterFamilyPrefs(pref_service_, list_family_members_response);

    for (const char* property : kCustodianInfoPrefs) {
      EXPECT_THAT(pref_service_.GetString(property), Not(IsEmpty()));
    }
  }

  {
    kidsmanagement::ListMembersResponse list_family_members_response;
    RegisterFamilyPrefs(pref_service_, list_family_members_response);
    for (const char* property : kCustodianInfoPrefs) {
      EXPECT_THAT(pref_service_.GetString(property), IsEmpty());
    }
  }
}

TEST_F(SupervisedUserPreferencesTest, IsSafeSitesEnabledSupervisedUser) {
  // Enables parental controls with safe sites checks.
  EnableParentalControls(pref_service_);
  EXPECT_TRUE(IsSafeSitesEnabled(pref_service_));
}

TEST_F(SupervisedUserPreferencesTest,
       IsSafeSitesEnabledIndependentlyFromSupervision) {
  // Default behavior.
  ASSERT_FALSE(IsSubjectToParentalControls(pref_service_));
  ASSERT_FALSE(IsSafeSitesEnabled(pref_service_));

  pref_service_.SetSupervisedUserPref(prefs::kSupervisedUserSafeSites,
                                      base::Value(true));
  ASSERT_FALSE(IsSubjectToParentalControls(pref_service_));
  EXPECT_TRUE(IsSafeSitesEnabled(pref_service_));
}

TEST_F(SupervisedUserPreferencesTest,
       IsSubjectToParentalControlsForSupervisedUser) {
  // Simply enables parental controls.
  EnableParentalControls(pref_service_);
  EXPECT_TRUE(supervised_user::IsSubjectToParentalControls(pref_service_));

  // Safe sites is enabled by default.
  EXPECT_TRUE(supervised_user::IsSafeSitesEnabled(pref_service_));
}

TEST_F(SupervisedUserPreferencesTest,
       IsSubjectToParentalControlsForNonSupervisedUser) {
  // Set non-supervised user preference.
  pref_service_.SetString(prefs::kSupervisedUserId, std::string());
  EXPECT_FALSE(IsSubjectToParentalControls(pref_service_));
}


}  // namespace
}  // namespace supervised_user
