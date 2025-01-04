// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/list_family_members_service.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
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
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
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
 public:
  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());
    test_list_family_members_service_ =
        std::make_unique<ListFamilyMembersService>(
            identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            pref_service_);
  }

 protected:
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
  std::unique_ptr<ListFamilyMembersService> test_list_family_members_service_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ListFamilyMembersServiceTest, FamilyFlowsFromFetcherToPreferences) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string hoh_username;
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        ASSERT_FALSE(response.members().empty());
        ASSERT_EQ("", hoh_username);
        hoh_username = response.members().at(0).profile().display_name();
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));
  test_list_family_members_service_->Init();

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");

  test_list_family_members_service_->Shutdown();
}


TEST_F(ListFamilyMembersServiceTest, FamilyRolePrefReflectsAccountCapability) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string hoh_username;
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        ASSERT_FALSE(response.members().empty());
        ASSERT_EQ("", hoh_username);
        hoh_username = response.members().at(0).profile().display_name();
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));
  test_list_family_members_service_->Init();

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");

  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            "family_manager");

  test_list_family_members_service_->Shutdown();
}

TEST_F(ListFamilyMembersServiceTest,
       RepeatingCallbackUpdatesPreferencesMultipleTimes) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string hoh_username;
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        ASSERT_FALSE(response.members().empty());
        hoh_username = response.members().at(0).profile().display_name();
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));
  test_list_family_members_service_->Init();

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");

  task_environment_.FastForwardBy(base::Days(2));

  // Perform another sequence of obtaining an access token, simulating response
  // and verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("another_username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "another_username_hoh");

  test_list_family_members_service_->Shutdown();
}

TEST_F(ListFamilyMembersServiceTest, IneligibleAccountForFamilyFetch) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string hoh_username;
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        ASSERT_FALSE(response.members().empty());
        hoh_username = response.members().at(0).profile().display_name();
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  test_list_family_members_service_->Init();

  // No requests made for ineligible account.
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  test_list_family_members_service_->Shutdown();
}

TEST_F(ListFamilyMembersServiceTest, AccountEligibilityUpdated) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string hoh_username;
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        ASSERT_FALSE(response.members().empty());
        hoh_username = response.members().at(0).profile().display_name();
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  test_list_family_members_service_->Init();

  // No requests made for ineligible account.
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  // Set the eligibility capability after the service has been started.
  AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");

  test_list_family_members_service_->Shutdown();
}

// Tests that the Family Info is correctly fetched if the supervised account
// is made primary after the extended account info has been fetched.
// Prevents regressions to b/350715351.
TEST_F(ListFamilyMembersServiceTest,
       ListFamilyFetcherOnMakingSupervisedUserAccountPrimary) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string response_hoh_username;
  const std::string child_email = "username@gmail.com";
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        ASSERT_FALSE(response.members().empty());
        response_hoh_username =
            response.members().at(0).profile().display_name();
      });

  // Subscribe to the mock method and start the service.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);
  test_list_family_members_service_->Init();

  // Make non-primary account available. No requests are triggered for this
  // account.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(child_email);
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  // Set the supervised user capability after the service has been started for
  // the current (non-primary) account.
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
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
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(response_hoh_username, "username_hoh");

  test_list_family_members_service_->Shutdown();
}

TEST_F(ListFamilyMembersServiceTest,
       FamilyFlowsFromFetcherToPreferencesWithFetchCapabilityAndError) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches.
  auto extract_empty_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        ASSERT_TRUE(response.members().empty());
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_empty_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));
  test_list_family_members_service_->Init();

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateErrorResponseForPendingRequest();
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole), "");

  test_list_family_members_service_->Shutdown();
}

// Data cleanup is only available for Windows, Mac and Linux
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
TEST_F(ListFamilyMembersServiceTest, ListFamilyFetcherClearsResponseOnSignout) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string hoh_username;
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        if (response.members().empty()) {
          hoh_username = "";
        } else {
          hoh_username = response.members().at(0).profile().display_name();
        }
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));
  test_list_family_members_service_->Init();

  // Perform the sequence of obtaining an access token, simulating response
  // and verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            "family_manager");

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(hoh_username, "");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            kDefaultEmptyFamilyMemberRole);

  test_list_family_members_service_->Shutdown();
}

TEST_F(ListFamilyMembersServiceTest, ListFamilyFetcherResetsPrefOnSignout) {
  // Mock of FamilyPreferencesService::SetFamily, taking the
  // list family response from fetches. We check if the response is correct at
  // the last step with `hoh_username`.
  std::string hoh_username;
  auto extract_hoh_display_name_from_response = base::BindLambdaForTesting(
      [&](const kidsmanagement::ListMembersResponse& response) {
        if (response.members().empty()) {
          hoh_username = "";
        } else {
          hoh_username = response.members().at(0).profile().display_name();
        }
      });

  // Subscribe to the mock method.
  base::CallbackListSubscription subscription =
      test_list_family_members_service_->SubscribeToSuccessfulFetches(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "username_hoh@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.UpdateAccountInfoForAccount(
      WithFamilyInfoFetching(primary_account));
  test_list_family_members_service_->Init();

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            "family_manager");

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(hoh_username, "");
  EXPECT_EQ(pref_service_.GetString(prefs::kFamilyLinkUserMemberRole),
            kDefaultEmptyFamilyMemberRole);

  test_list_family_members_service_->Shutdown();
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace supervised_user
