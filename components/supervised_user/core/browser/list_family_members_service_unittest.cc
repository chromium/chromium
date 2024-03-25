// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/list_family_members_service.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class ListFamilyMembersServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_list_family_members_service_ =
        std::make_unique<supervised_user::ListFamilyMembersService>(
            identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_));
  }

 protected:
  void SimulateResponseForPendingRequest(std::string_view username) {
    kidsmanagement::ListMembersResponse response;
    supervised_user::SetFamilyMemberAttributesForTesting(
        response.add_members(), kidsmanagement::HEAD_OF_HOUSEHOLD, username);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/families/"
        "mine/members?alt=proto",
        response.SerializeAsString());
  }

  // Must be first attribute, see base::test::TaskEnvironment docs.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<supervised_user::ListFamilyMembersService>
      test_list_family_members_service_;
};

TEST_F(ListFamilyMembersServiceTest, FamilyFlowsFromFetcherToPreferences) {
  // Mock of supervised_user::FamilyPreferencesService::SetFamily, taking the
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
  identity_test_env_.MakePrimaryAccountAvailable("user_child@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  test_list_family_members_service_->Start();

  // Perform the sequence of obtaining an access token, simulating response and
  // verifying the result.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");

  test_list_family_members_service_->Cancel();
}

TEST_F(ListFamilyMembersServiceTest,
       RepeatingCallbackUpdatesPreferencesMultipleTimes) {
  // Mock of supervised_user::FamilyPreferencesService::SetFamily, taking the
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
  identity_test_env_.MakePrimaryAccountAvailable("user_child@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  test_list_family_members_service_->Start();

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

  test_list_family_members_service_->Cancel();
}

TEST_F(ListFamilyMembersServiceTest, OnceCallbacksAreDisposable) {
  // Mock of supervised_user::FamilyPreferencesService::SetFamily, taking the
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
      test_list_family_members_service_->SubscribeToNextSuccessfulFetch(
          extract_hoh_display_name_from_response);

  // Test the `fetcher_`.
  identity_test_env_.MakePrimaryAccountAvailable("user_child@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  test_list_family_members_service_->Start();

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
  // and verifying the result. This time, the `hoh_username` value will not be
  // updated because the callback was disposed after first use.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  SimulateResponseForPendingRequest("another_username_hoh");
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_EQ(hoh_username, "username_hoh");

  test_list_family_members_service_->Cancel();
}
