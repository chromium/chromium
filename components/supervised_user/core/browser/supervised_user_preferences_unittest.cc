// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_preferences.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
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
  void TearDown() override { supervised_user_test_environment_.Shutdown(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment supervised_user_test_environment_;
};

TEST_F(SupervisedUserPreferencesTest, RegisterProfilePrefsAndCheckDefaults) {
  // Checks the preference registration from the Setup.
  EXPECT_EQ(supervised_user_test_environment_.pref_service()->GetInteger(
                prefs::kDefaultSupervisedUserFilteringBehavior),
            static_cast<int>(FilteringBehavior::kAllow));

  // This is browser's default kSupervisedUserSafeSites setting (for
  // unsupervised user).
  EXPECT_EQ(supervised_user_test_environment_.pref_service()->GetBoolean(
                prefs::kSupervisedUserSafeSites),
            false);
  EXPECT_FALSE(IsSubjectToParentalControls(
      *supervised_user_test_environment_.pref_service()));
  // TODO(b/306376651): When we migrate more preference reading methods in this
  // library, add more test cases for their correct default values.
}

TEST_F(SupervisedUserPreferencesTest, ToggleParentalControlsSetsUserId) {
  EnableParentalControls(*supervised_user_test_environment_.pref_service());
  EXPECT_EQ(supervised_user_test_environment_.pref_service()->GetString(
                prefs::kSupervisedUserId),
            kChildAccountSUID);

  DisableParentalControls(*supervised_user_test_environment_.pref_service());
  EXPECT_EQ(supervised_user_test_environment_.pref_service()->GetString(
                prefs::kSupervisedUserId),
            std::string());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SupervisedUserPreferencesTest, ToggleParentalControlsSetsChildStatus) {
  EnableParentalControls(*supervised_user_test_environment_.pref_service());
  EXPECT_TRUE(IsChildAccountStatusKnown(
      *supervised_user_test_environment_.pref_service()));

  DisableParentalControls(*supervised_user_test_environment_.pref_service());
  EXPECT_TRUE(IsChildAccountStatusKnown(
      *supervised_user_test_environment_.pref_service()));
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

  RegisterFamilyPrefs(*supervised_user_test_environment_.pref_service(),
                      list_family_members_response);

  EXPECT_EQ("username_hoh",
            supervised_user_test_environment_.pref_service()->GetString(
                prefs::kSupervisedUserCustodianName));
  EXPECT_EQ("username_parent",
            supervised_user_test_environment_.pref_service()->GetString(
                prefs::kSupervisedUserSecondCustodianName));
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

    RegisterFamilyPrefs(*supervised_user_test_environment_.pref_service(),
                        list_family_members_response);

    for (const char* property : kCustodianInfoPrefs) {
      EXPECT_THAT(
          supervised_user_test_environment_.pref_service()->GetString(property),
          Not(IsEmpty()));
    }
  }

  {
    kidsmanagement::ListMembersResponse list_family_members_response;
    RegisterFamilyPrefs(*supervised_user_test_environment_.pref_service(),
                        list_family_members_response);
    for (const char* property : kCustodianInfoPrefs) {
      EXPECT_THAT(
          supervised_user_test_environment_.pref_service()->GetString(property),
          IsEmpty());
    }
  }
}

TEST_F(SupervisedUserPreferencesTest,
       IsSubjectToParentalControlsForNonSupervisedUser) {
  // Set non-supervised user preference.
  supervised_user_test_environment_.pref_service()->SetString(
      prefs::kSupervisedUserId, std::string());
  EXPECT_FALSE(IsSubjectToParentalControls(
      *supervised_user_test_environment_.pref_service()));
}
}  // namespace
}  // namespace supervised_user
