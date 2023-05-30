// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_preferences_service.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/supervised_user/core/browser/kids_chrome_management_test_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::IsEmpty;
using ::testing::Not;

}  // namespace

class FamilyPreferencesServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    pref_service_.registry()->RegisterStringPref(prefs::kSupervisedUserId,
                                                 std::string());
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kChildAccountStatusKnown, false);
    for (const char* pref : supervised_user::kCustodianInfoPrefs) {
      pref_service_.registry()->RegisterStringPref(pref, std::string());
    }
    test_family_preferences_service_ =
        std::make_unique<supervised_user::FamilyPreferencesService>(
            &pref_service_);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<supervised_user::FamilyPreferencesService>
      test_family_preferences_service_;
};

TEST_F(FamilyPreferencesServiceTest, ToggleParentalControls) {
  test_family_preferences_service_->EnableParentalControls();
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserId),
            supervised_user::kChildAccountSUID);
  EXPECT_TRUE(test_family_preferences_service_->IsChildAccountStatusKnown());

  test_family_preferences_service_->DisableParentalControls();
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserId), std::string());
  EXPECT_TRUE(test_family_preferences_service_->IsChildAccountStatusKnown());
}

TEST_F(FamilyPreferencesServiceTest, StartFetchingFamilyInfo) {
  kids_chrome_management::ListFamilyMembersResponse
      list_family_members_response;
  supervised_user::SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(),
      kids_chrome_management::HEAD_OF_HOUSEHOLD, "username_hoh");
  supervised_user::SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(),
      kids_chrome_management::PARENT, "username_parent");
  supervised_user::SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(), kids_chrome_management::CHILD,
      "username_child");
  supervised_user::SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(),
      kids_chrome_management::MEMBER, "username_member");

  test_family_preferences_service_->SetFamily(list_family_members_response);

  EXPECT_EQ("username_hoh",
            pref_service_.GetString(prefs::kSupervisedUserCustodianName));
  EXPECT_EQ("username_parent",
            pref_service_.GetString(prefs::kSupervisedUserSecondCustodianName));
}

TEST_F(FamilyPreferencesServiceTest, FieldsAreClearedForNonChildAccounts) {
  {
    kids_chrome_management::ListFamilyMembersResponse
        list_family_members_response;
    supervised_user::SetFamilyMemberAttributesForTesting(
        list_family_members_response.add_members(),
        kids_chrome_management::HEAD_OF_HOUSEHOLD, "username_hoh");
    supervised_user::SetFamilyMemberAttributesForTesting(
        list_family_members_response.add_members(),
        kids_chrome_management::PARENT, "username_parent");

    test_family_preferences_service_->SetFamily(list_family_members_response);

    for (const char* property : supervised_user::kCustodianInfoPrefs) {
      EXPECT_THAT(pref_service_.GetString(property), Not(IsEmpty()));
    }
  }

  {
    kids_chrome_management::ListFamilyMembersResponse
        list_family_members_response;
    test_family_preferences_service_->SetFamily(list_family_members_response);
    for (const char* property : supervised_user::kCustodianInfoPrefs) {
      EXPECT_THAT(pref_service_.GetString(property), IsEmpty());
    }
  }
}
