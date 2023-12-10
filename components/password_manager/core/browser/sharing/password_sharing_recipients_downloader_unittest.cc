// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_sharing_recipients_downloader.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/password_sharing_recipients.pb.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::PasswordSharingRecipientsResponse;
using testing::Invoke;
using testing::NotNull;

namespace password_manager {
namespace {

PasswordSharingRecipientsResponse MakeDefaultResponse() {
  PasswordSharingRecipientsResponse response;
  response.set_result(PasswordSharingRecipientsResponse::SUCCESS);
  return response;
}

class PasswordSharingRecipientsDownloaderTest : public testing::Test {
 public:
  PasswordSharingRecipientsDownloaderTest() {
    identity_env_.MakePrimaryAccountAvailable("test@email.com",
                                              signin::ConsentLevel::kSync);
    identity_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  std::unique_ptr<PasswordSharingRecipientsDownloader> CreateDownloader() {
    return std::make_unique<PasswordSharingRecipientsDownloader>(
        version_info::Channel::DEFAULT,
        test_url_loader_factory_.GetSafeWeakWrapper(),
        identity_env_.identity_manager());
  }

  void SetResponse(const PasswordSharingRecipientsResponse& response) {
    test_url_loader_factory_.AddResponse(
        PasswordSharingRecipientsDownloader::GetPasswordSharingRecipientsURL(
            version_info::Channel::DEFAULT)
            .spec(),
        response.SerializeAsString());
  }

  signin::IdentityTestEnvironment* identity_env() { return &identity_env_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_env_;
};

TEST_F(PasswordSharingRecipientsDownloaderTest, ShouldDownloadRecipients) {
  base::RunLoop run_loop;

  SetResponse(MakeDefaultResponse());

  std::unique_ptr<PasswordSharingRecipientsDownloader> downloader =
      CreateDownloader();
  downloader->Start(run_loop.QuitClosure());
  run_loop.Run();

  std::optional<PasswordSharingRecipientsResponse> response =
      downloader->TakeResponse();
  ASSERT_TRUE(response);
  EXPECT_EQ(response->result(), PasswordSharingRecipientsResponse::SUCCESS);
  EXPECT_EQ(downloader->GetHttpError(), net::HTTP_OK);
  EXPECT_EQ(downloader->GetNetError(), net::OK);
}

TEST_F(PasswordSharingRecipientsDownloaderTest,
       ShouldRetryAccessTokenFetchOnceOnTransientError) {
  identity_env()->SetAutomaticIssueOfAccessTokens(false);

  base::MockOnceClosure callback;
  std::unique_ptr<PasswordSharingRecipientsDownloader> downloader =
      CreateDownloader();
  downloader->Start(callback.Get());

  // Return a transient error on access token request and verify that the
  // `callback` hasn't been called yet.
  EXPECT_CALL(callback, Run).Times(0);
  ASSERT_TRUE(identity_env()->IsAccessTokenRequestPending());
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_EQ(downloader->GetAuthError().state(),
            GoogleServiceAuthError::CONNECTION_FAILED);

  // Return another transient error on access token request, the `callback`
  // should be called this time.
  EXPECT_CALL(callback, Run);
  ASSERT_TRUE(identity_env()->IsAccessTokenRequestPending());
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

  EXPECT_EQ(downloader->GetAuthError().state(),
            GoogleServiceAuthError::CONNECTION_FAILED);
}

}  // namespace
}  // namespace password_manager
