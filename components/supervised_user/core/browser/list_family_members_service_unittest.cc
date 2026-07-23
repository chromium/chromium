// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/list_family_members_service.h"

#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

const char kListMembersRequestPath[] =
    "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/families/"
    "mine/members?alt=proto&allow_empty_family=true";

// Configures the account_info so that ListFamilyMembersService will fetch
// family info for that account.
AccountInfo& WithFamilyInfoFetching(AccountInfo& account_info) {
  AccountCapabilitiesTestMutator mutator(&account_info);
  if (FetchListFamilyMembersWithCapability()) {
    mutator.set_can_fetch_family_member_info(true);
    mutator.set_is_subject_to_parental_controls(false);
  } else {
    mutator.set_can_fetch_family_member_info(false);
    mutator.set_is_subject_to_parental_controls(true);
  }
  return account_info;
}

class ListFamilyMembersServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());
    under_test_ = std::make_unique<ListFamilyMembersService>(
        *identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        pref_service_);

  }

  void TearDown() override { under_test_->Shutdown(); }

  void SimulateErrorResponseForPendingRequest() {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        kListMembersRequestPath, /*content=*/"", net::HTTP_BAD_REQUEST);
  }

  void SimulateEmptyResponseForPendingRequest() {
    kidsmanagement::ListMembersResponse response;
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        kListMembersRequestPath, response.SerializeAsString());
  }

  void SimulateResponseForPendingRequest(std::string_view username) {
    kidsmanagement::ListMembersResponse response;
    SetFamilyMemberAttributesForTesting(
        response.add_members(), kidsmanagement::HEAD_OF_HOUSEHOLD, username);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        kListMembersRequestPath, response.SerializeAsString());
  }

  // Must be first attribute, see base::test::TaskEnvironment docs.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<ListFamilyMembersService> under_test_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ListFamilyMembersServiceTest, FamilyFlowsFromFetcherToPreferences) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");
}

TEST_F(ListFamilyMembersServiceTest, FamilyRolePrefReflectsAccountCapability) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&primary_account);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            "family_manager");
}

TEST_F(ListFamilyMembersServiceTest,
       RepeatingCallbackUpdatesPreferencesMultipleTimes) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&primary_account);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");

  task_environment_.FastForwardBy(base::Days(2));

  // Perform another sequence of obtaining an access token, simulating response
  // and verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");
  SimulateResponseForPendingRequest("another_username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "another_username_hoh");
}

TEST_F(ListFamilyMembersServiceTest, IneligibleAccountForFamilyFetch) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);

  // No requests made for ineligible account.
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(ListFamilyMembersServiceTest, AccountEligibilityUpdated) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);

  // No requests made for ineligible account.
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  // Set the eligibility capability after the service has been started.
  AccountCapabilitiesTestMutator mutator(&primary_account);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");
}

// Tests that the Family Info is correctly fetched if the supervised account
// is made primary after the extended account info has been fetched.
// Prevents regressions to b/350715351.
TEST_F(ListFamilyMembersServiceTest,
       ListFamilyFetcherOnMakingSupervisedUserAccountPrimary) {
  const std::string child_email = "username@gmail.com";

  // Make non-primary account available. No requests are triggered for this
  // account.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(child_email);
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  // Set the supervised user capability after the service has been started for
  // the current (non-primary) account.
  AccountCapabilitiesTestMutator mutator(&account_info);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(account_info));
  // No requests made for ineligible account.
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  // Make the account primary.
  identity_test_env_.SetPrimaryAccount(child_email,
                                       signin::ConsentLevel::kSignin);

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");
}

TEST_F(ListFamilyMembersServiceTest,
       FamilyFlowsFromFetcherToPreferencesWithFetchCapabilityAndError) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&primary_account);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateErrorResponseForPendingRequest();
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole), "");
}

// Data cleanup is only available for Windows, Mac and Linux
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
TEST_F(ListFamilyMembersServiceTest, ListFamilyFetcherClearsResponseOnSignout) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response
  // and verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            "family_manager");

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            kDefaultEmptyFamilyMemberRole);
}

TEST_F(ListFamilyMembersServiceTest, ListFamilyFetcherResetsPrefOnSignout) {
  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName),
            "username_hoh");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            "family_manager");

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(pref_service_.GetString(prefs::kSupervisedUserCustodianName), "");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            kDefaultEmptyFamilyMemberRole);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace supervised_user
