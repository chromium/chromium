// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "components/omnibox/composebox/test_composebox_query_controller.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kSessionIdQueryParameterKey[] = "gsessionid";
constexpr char kTestUser[] = "test_user@gmail.com";
constexpr char kTestServerSessionId[] = "test_server_session_id";

class ComposeboxQueryControllerTest
    : public testing::Test,
      public ComposeboxQueryController::FileUploadStatusObserver {
 public:
  ComposeboxQueryControllerTest() = default;
  ~ComposeboxQueryControllerTest() override = default;

  void SetUp() override {
    num_file_upload_status_changed_calls_ = 0;
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);

    controller_ = std::make_unique<TestComposeboxQueryController>(
        identity_manager(), shared_url_loader_factory_,
        version_info::Channel::UNKNOWN);
    controller_->AddObserver(this);

    lens::LensOverlayServerClusterInfoResponse cluster_info_response;
    cluster_info_response.set_server_session_id(kTestServerSessionId);
    controller_->set_fake_cluster_info_response(cluster_info_response);
  }

  void TearDown() override { controller_->RemoveObserver(this); }

  TestComposeboxQueryController& controller() { return *controller_; }

  // ComposeboxQueryController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& client_token,
      FileUploadStatus file_upload_status,
      const std::optional<std::string>& error_message) override {
    num_file_upload_status_changed_calls_++;
    if (expected_client_token_.has_value() &&
        expected_client_token_.value() == client_token &&
        expected_file_upload_status_.has_value() &&
        expected_file_upload_status_.value() == file_upload_status) {
      file_upload_status_run_loop_.Quit();
      expected_client_token_.reset();
      expected_file_upload_status_.reset();
    }
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  // Returns an AccessTokenInfo with valid information that can be used for
  // completing access token requests.
  const signin::AccessTokenInfo& access_token_info() const {
    return access_token_info_;
  }

  // The number of times the file upload status changed callback has been
  // called.
  int num_file_upload_status_changed_calls_ = 0;

  // Sets the expected client token and file upload status to watch for file
  // upload status changes for toggling run loops. Call
  // WaitForFileExpectedUploadStatus() afterwards to run the run loop.
  // TODO(crbug.com/427759049): Consider using base::TestFuture instead of
  // run loops.
  void SetExpectedFileUploadStatus(const base::UnguessableToken& client_token,
                                   FileUploadStatus file_upload_status) {
    expected_client_token_ = client_token;
    expected_file_upload_status_ = file_upload_status;
  }

  // Runs the file upload status run loop.
  void WaitForFileExpectedUploadStatus() { file_upload_status_run_loop_.Run(); }

  // Returns the gsessionid parameter from the given url.
  std::optional<std::string> GetGsessionIdFromUrl(GURL url) {
    std::string gsessionid_param;
    bool has_gsessionid_param = net::GetValueForKeyInQuery(
        url, kSessionIdQueryParameterKey, &gsessionid_param);
    if (has_gsessionid_param) {
      return gsessionid_param;
    }
    return std::nullopt;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<TestComposeboxQueryController> controller_;
  signin::AccessTokenInfo access_token_info_{"access_token", base::Time::Max(),
                                             "id_token"};

  // The expected client token to watch for file upload status changes for
  // toggling run loops.
  std::optional<base::UnguessableToken> expected_client_token_;

  // The expected file upload status to watch for file upload status changes
  // for toggling run loops.
  std::optional<FileUploadStatus> expected_file_upload_status_;

  // The run loop to quit when the file upload status becomes the expected
  // status.
  base::RunLoop file_upload_status_run_loop_;
};

TEST_F(ComposeboxQueryControllerTest, NotifySessionStarted) {
  controller().NotifySessionStarted();
  EXPECT_EQ(SessionState::kSessionStarted, controller().session_state());
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequest) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();
  run_loop.Run();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequestWithOAuth) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
  run_loop.Run();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
}

TEST_F(ComposeboxQueryControllerTest,
       NotifySessionStartedIssuesClusterInfoRequestFailure) {
  // Wait until the state changes to kClusterInfoInvalid.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoInvalid) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().set_next_cluster_info_request_should_return_error(true);
  controller().NotifySessionStarted();
  run_loop.Run();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoInvalid,
            controller().query_controller_state());
}

TEST_F(ComposeboxQueryControllerTest, NotifySessionAbandoned) {
  controller().NotifySessionAbandoned();
  EXPECT_EQ(SessionState::kSessionAbandoned, controller().session_state());
}

TEST_F(ComposeboxQueryControllerTest,
       UploadFileRequestTriggersClientProcessing) {
  // Start the session.
  controller().NotifySessionStarted();

  // Add file to cache.
  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  const base::UnguessableToken client_token = base::UnguessableToken::Create();
  file_info->client_token_ = client_token;
  file_info->mime_type_ = lens::MimeType::kImage;

  SetExpectedFileUploadStatus(client_token,
                              FileUploadStatus::kClientProcessing);

  controller().StartFileUploadFlow(
      std::move(file_info),
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  WaitForFileExpectedUploadStatus();
}

TEST_F(ComposeboxQueryControllerTest, UploadFileRequestFailure) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();
  run_loop.Run();

  // Add file to cache.
  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  const base::UnguessableToken client_token = base::UnguessableToken::Create();
  file_info->client_token_ = client_token;
  file_info->mime_type_ = lens::MimeType::kImage;

  SetExpectedFileUploadStatus(client_token, FileUploadStatus::kUploadFailed);

  controller().set_next_file_upload_request_should_return_error(true);
  controller().StartFileUploadFlow(
      std::move(file_info),
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  WaitForFileExpectedUploadStatus();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
  EXPECT_EQ(controller().num_file_upload_requests_sent(), 1);
  EXPECT_EQ(num_file_upload_status_changed_calls_, 3);
  EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
              testing::Optional(std::string(kTestServerSessionId)));
}

TEST_F(ComposeboxQueryControllerTest, UploadImageFileRequestSuccess) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();
  run_loop.Run();

  // Add file to cache.
  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  const base::UnguessableToken client_token = base::UnguessableToken::Create();
  file_info->client_token_ = client_token;
  file_info->mime_type_ = lens::MimeType::kImage;

  SetExpectedFileUploadStatus(client_token,
                              FileUploadStatus::kUploadSuccessful);

  controller().StartFileUploadFlow(
      std::move(file_info),
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  WaitForFileExpectedUploadStatus();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
  EXPECT_EQ(controller().num_file_upload_requests_sent(), 1);
  EXPECT_EQ(num_file_upload_status_changed_calls_, 3);
  EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
              testing::Optional(std::string(kTestServerSessionId)));
  EXPECT_EQ(controller().GetFileInfo(client_token)->GetFileUploadStatus(),
            FileUploadStatus::kUploadSuccessful);

  // Check that the vsrid matches that for an image upload.
  EXPECT_EQ(controller()
                .GetFileInfo(client_token)
                ->GetRequestIdForTesting()
                ->sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfo(client_token)
                ->GetRequestIdForTesting()
                ->image_sequence_id(),
            1);
}

TEST_F(ComposeboxQueryControllerTest, UploadPdfFileRequestSuccess) {
  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();
  run_loop.Run();

  // Add file to cache.
  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  const base::UnguessableToken client_token = base::UnguessableToken::Create();
  file_info->client_token_ = client_token;
  file_info->mime_type_ = lens::MimeType::kPdf;

  SetExpectedFileUploadStatus(client_token,
                              FileUploadStatus::kUploadSuccessful);

  controller().StartFileUploadFlow(
      std::move(file_info),
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  WaitForFileExpectedUploadStatus();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
  EXPECT_EQ(controller().num_file_upload_requests_sent(), 1);
  EXPECT_EQ(num_file_upload_status_changed_calls_, 3);
  EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
              testing::Optional(std::string(kTestServerSessionId)));
  EXPECT_EQ(controller().GetFileInfo(client_token)->GetFileUploadStatus(),
            FileUploadStatus::kUploadSuccessful);

  // Check that the vsrid matches that for a pdf upload.
  EXPECT_EQ(controller()
                .GetFileInfo(client_token)
                ->GetRequestIdForTesting()
                ->sequence_id(),
            1);
  EXPECT_EQ(controller()
                .GetFileInfo(client_token)
                ->GetRequestIdForTesting()
                ->image_sequence_id(),
            1);
}

TEST_F(ComposeboxQueryControllerTest, UploadFileRequestSuccessWithOAuth) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();

  // Send the access token for the cluster info request.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
  run_loop.Run();

  // Add file to cache.
  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  const base::UnguessableToken client_token = base::UnguessableToken::Create();
  file_info->client_token_ = client_token;
  file_info->mime_type_ = lens::MimeType::kImage;

  SetExpectedFileUploadStatus(client_token,
                              FileUploadStatus::kUploadSuccessful);

  controller().StartFileUploadFlow(
      std::move(file_info),
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  // Send the access token for the file upload request.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  WaitForFileExpectedUploadStatus();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
  EXPECT_EQ(controller().num_file_upload_requests_sent(), 1);
  EXPECT_EQ(num_file_upload_status_changed_calls_, 3);
  EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
              testing::Optional(std::string(kTestServerSessionId)));
}

TEST_F(ComposeboxQueryControllerTest,
       UploadFileRequestWithOAuthAndDelayedClusterInfo) {
  identity_test_env()->MakePrimaryAccountAvailable(
      kTestUser, signin::ConsentLevel::kSignin);

  // Wait until the state changes to kClusterInfoReceived.
  base::RunLoop cluster_info_run_loop;
  controller().set_on_query_controller_state_changed_callback(
      base::BindLambdaForTesting([&](QueryControllerState state) {
        if (state == QueryControllerState::kClusterInfoReceived) {
          cluster_info_run_loop.Quit();
        }
      }));

  // Start the session.
  controller().NotifySessionStarted();

  // Add file to cache.
  std::unique_ptr<ComposeboxQueryController::FileInfo> file_info =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  const base::UnguessableToken client_token = base::UnguessableToken::Create();
  file_info->client_token_ = client_token;
  file_info->mime_type_ = lens::MimeType::kImage;

  // Start the file upload flow without waiting for the cluster info request to
  // complete.
  controller().StartFileUploadFlow(
      std::move(file_info),
      /*file_data=*/base::MakeRefCounted<base::RefCountedBytes>());

  EXPECT_EQ(QueryControllerState::kAwaitingClusterInfoResponse,
            controller().query_controller_state());

  SetExpectedFileUploadStatus(client_token,
                              FileUploadStatus::kUploadSuccessful);

  // Send the oauth token for the cluster info or file upload request.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Send the oauth token for the other request.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // Finally wait for the cluster info request and file upload to complete.
  cluster_info_run_loop.Run();
  WaitForFileExpectedUploadStatus();

  // Validate.
  EXPECT_EQ(controller().num_cluster_info_fetch_requests_sent(), 1);
  EXPECT_EQ(QueryControllerState::kClusterInfoReceived,
            controller().query_controller_state());
  EXPECT_EQ(controller().num_file_upload_requests_sent(), 1);
  EXPECT_EQ(num_file_upload_status_changed_calls_, 3);
  EXPECT_THAT(GetGsessionIdFromUrl(controller().last_sent_fetch_url()),
              testing::Optional(std::string(kTestServerSessionId)));
  EXPECT_EQ(controller().GetFileInfo(client_token)->GetFileUploadStatus(),
            FileUploadStatus::kUploadSuccessful);
}
