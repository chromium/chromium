// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/password_sharing_recipients.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::StrictMock;

// TODO(crbug.com/40272762): Move this to a common test helper to simplify the
// setup in tests and the maintenance later.
void SetupIdentityEnvironment(
    raw_ptr<signin::IdentityTestEnvironment> identity_test_env) {
  identity_test_env->MakePrimaryAccountAvailable("test@email.com",
                                                 signin::ConsentLevel::kSync);
  identity_test_env->SetAutomaticIssueOfAccessTokens(true);
}

class RecipientsFetcherImplTest : public testing::Test {
 public:
  RecipientsFetcherImplTest() { SetupIdentityEnvironment(&identity_test_env_); }
  ~RecipientsFetcherImplTest() override = default;

  RecipientsFetcherImpl CreateRecipientFetcher() {
    return RecipientsFetcherImpl(version_info::Channel::DEFAULT,
                                 test_url_loader_factory_.GetSafeWeakWrapper(),
                                 identity_test_env_.identity_manager());
  }

  void SetServerResponse(
      const sync_pb::PasswordSharingRecipientsResponse& response) {
    test_url_loader_factory_.AddResponse(
        PasswordSharingRecipientsDownloader::GetPasswordSharingRecipientsURL(
            version_info::Channel::DEFAULT)
            .spec(),
        response.SerializeAsString());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
};

// Tests the happy case in which the server returns a potential candidate to
// share a password with. This is done by pre-configuring the expected server
// response. In a real environment the user that sends the request and the
// returned candidate also needs to be in the same family circle.
TEST_F(RecipientsFetcherImplTest, ShouldFetchRecipientInfoWhenRequestSucceeds) {
  const std::string kTestUserId = "12345";
  const std::string kTestUserName = "Theo Tester";
  const std::string kTestEmail = "theo@example.com";
  const std::string kTestProfileImageUrl =
      "https://3837fjsdjaka.image.example.com";
  const std::string kTestPublicKey = "01234567890123456789012345678912";
  const std::string kTestPublicKeyBase64 =
      "MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MTI=";
  const uint32_t kTestPublicKeyVersion = 0;

  // Create the server response.
  sync_pb::PasswordSharingRecipientsResponse response;
  response.set_result(sync_pb::PasswordSharingRecipientsResponse::SUCCESS);

  sync_pb::UserInfo* user_info = response.add_recipients();
  user_info->set_user_id(kTestUserId);
  user_info->mutable_user_display_info()->set_display_name(kTestUserName);
  user_info->mutable_user_display_info()->set_email(kTestEmail);
  user_info->mutable_user_display_info()->set_profile_image_url(
      kTestProfileImageUrl);
  user_info->mutable_cross_user_sharing_public_key()->set_x25519_public_key(
      kTestPublicKey);
  user_info->mutable_cross_user_sharing_public_key()->set_version(
      kTestPublicKeyVersion);

  SetServerResponse(response);

  // Set up the expected callback.
  RecipientInfo expected_recipient_info;
  expected_recipient_info.user_id = kTestUserId;
  expected_recipient_info.user_name = kTestUserName;
  expected_recipient_info.email = kTestEmail;
  expected_recipient_info.profile_image_url = kTestProfileImageUrl;
  expected_recipient_info.public_key.key = kTestPublicKeyBase64;
  expected_recipient_info.public_key.key_version = kTestPublicKeyVersion;
  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback;
  EXPECT_CALL(callback, Run(ElementsAre(expected_recipient_info),
                            Eq(FetchFamilyMembersRequestStatus::kSuccess)));

  // Create the RecipientsFetcher and make a request.
  RecipientsFetcherImpl recipient_fetcher = CreateRecipientFetcher();
  recipient_fetcher.FetchFamilyMembers(callback.Get());

  RunUntilIdle();
}

// Tests the scenario in which the sender of the request is not part of a family
// circle. This is simulated by pre-configuring the expected server response.
TEST_F(RecipientsFetcherImplTest,
       ShouldReturnNotFamilyMemberStatusIfUserIsNotInFamilyCircle) {
  sync_pb::PasswordSharingRecipientsResponse response;
  response.set_result(
      sync_pb::PasswordSharingRecipientsResponse::NOT_FAMILY_MEMBER);
  SetServerResponse(response);

  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback;
  EXPECT_CALL(callback,
              Run(IsEmpty(), Eq(FetchFamilyMembersRequestStatus::kNoFamily)));

  RecipientsFetcherImpl recipient_fetcher = CreateRecipientFetcher();
  recipient_fetcher.FetchFamilyMembers(callback.Get());

  RunUntilIdle();
}

// Tests the scenario in which the sender of the request is the only member of a
// family circle. This is simulated by pre-configuring the expected server
// response.
TEST_F(RecipientsFetcherImplTest,
       ShouldReturnNoOtherFamilyMembersStatusIfUserIsTheOnlyMember) {
  sync_pb::PasswordSharingRecipientsResponse response;
  response.set_result(sync_pb::PasswordSharingRecipientsResponse::SUCCESS);
  SetServerResponse(response);

  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback;
  EXPECT_CALL(callback,
              Run(IsEmpty(),
                  Eq(FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers)));

  RecipientsFetcherImpl recipient_fetcher = CreateRecipientFetcher();
  recipient_fetcher.FetchFamilyMembers(callback.Get());

  RunUntilIdle();
}

TEST_F(RecipientsFetcherImplTest,
       ShouldReturnUnknownStatusIfSyncRequestHasUnknownError) {
  sync_pb::PasswordSharingRecipientsResponse response;
  response.set_result(sync_pb::PasswordSharingRecipientsResponse::UNKNOWN);
  SetServerResponse(response);

  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback;
  EXPECT_CALL(callback,
              Run(IsEmpty(), Eq(FetchFamilyMembersRequestStatus::kUnknown)));

  RecipientsFetcherImpl recipient_fetcher = CreateRecipientFetcher();
  recipient_fetcher.FetchFamilyMembers(callback.Get());

  RunUntilIdle();
}

// Tests that there can only be a single request active at any point in time.
// Concurrent requests will be instantly completed with the request status set
// to kPendingRequest.
TEST_F(RecipientsFetcherImplTest, ShouldNotFetchRecipientIfPendingRequest) {
  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback;
  EXPECT_CALL(
      callback,
      Run(_, Eq(FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers)));

  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback2;
  EXPECT_CALL(
      callback2,
      Run(IsEmpty(), Eq(FetchFamilyMembersRequestStatus::kPendingRequest)));

  RecipientsFetcherImpl recipient_fetcher = CreateRecipientFetcher();
  recipient_fetcher.FetchFamilyMembers(callback.Get());
  recipient_fetcher.FetchFamilyMembers(callback2.Get());

  sync_pb::PasswordSharingRecipientsResponse response;
  response.set_result(sync_pb::PasswordSharingRecipientsResponse::SUCCESS);
  SetServerResponse(response);

  RunUntilIdle();
}

TEST_F(RecipientsFetcherImplTest,
       ShouldFetchRecpientInfoWhenRequestSucceedsForConsecutiveRequests) {
  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback;
  EXPECT_CALL(
      callback,
      Run(_, Eq(FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers)));

  StrictMock<base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback>>
      callback2;
  EXPECT_CALL(
      callback2,
      Run(_, Eq(FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers)));

  RecipientsFetcherImpl recipient_fetcher = CreateRecipientFetcher();
  recipient_fetcher.FetchFamilyMembers(callback.Get());

  sync_pb::PasswordSharingRecipientsResponse response;
  response.set_result(sync_pb::PasswordSharingRecipientsResponse::SUCCESS);
  SetServerResponse(response);

  RunUntilIdle();

  recipient_fetcher.FetchFamilyMembers(callback2.Get());
  RunUntilIdle();
}

TEST_F(RecipientsFetcherImplTest, FetchRecipientDuringAuthError) {
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);
  base::MockCallback<RecipientsFetcher::FetchFamilyMembersCallback> callback;

  // Create the RecipientsFetcher and make a request.
  RecipientsFetcherImpl recipient_fetcher = CreateRecipientFetcher();
  recipient_fetcher.FetchFamilyMembers(callback.Get());

  // Simulate the permanent Auth error.
  EXPECT_CALL(
      callback,
      Run(IsEmpty(), Eq(FetchFamilyMembersRequestStatus::kNetworkError)));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  RunUntilIdle();

  // Double check that there were no network requests to the server.
  EXPECT_EQ(test_url_loader_factory()->total_requests(), 0u);
}

}  // namespace
}  // namespace password_manager
