// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/enterprise/client_certificates/core/dm_server_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace enterprise_attestation {
namespace {

class DMServerClientUnitTest : public testing::Test {
 public:
  DMServerClientUnitTest() = default;
  ~DMServerClientUnitTest() override = default;

  void InitializeUploader() {
    service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
    dm_server_client_ =
        DMServerClient::Create(&service_, shared_url_loader_factory_);
  }

  void SetSuccessfulJobReply(unsigned number_of_calls = 1) {
    // Return a response with a pem certificate.
    enterprise_management::DeviceManagementResponse response;
    response.mutable_browser_public_key_upload_response()
        ->set_pem_encoded_certificate("pem encoded certificate!");
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .Times(number_of_calls)
        .WillRepeatedly(DoAll(service_.SendJobOKAsync(response),
                              service_.CaptureJobType(&captured_job_type_),
                              service_.CaptureRequest(&captured_request_)));
  }

  void SetFailedJobReply(int net_error, int response_code) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(
            DoAll(service_.SendJobResponseAsync(net_error, response_code)))
        .RetiresOnSaturation();
  }

  void OnUploadJobCompleted(const std::string& key,
                            base::OnceClosure finish_callback,
                            policy::DMServerJobResult result) {
    upload_results_[key] = result;
    histogram_tester_.ExpectUniqueSample(dm_server_client_->kNetErrorHistogram,
                                         result.net_error,
                                         result.net_error == net::OK ? 0 : 1);

    std::move(finish_callback).Run();
  }

  void SyncUploadBrowserPublicKey(
      const std::string& client_id,
      const std::string& dm_token,
      const std::optional<std::string>& profile_id,
      const enterprise_management::DeviceManagementRequest& request,
      const std::string& key = default_key) {
    base::RunLoop run_loop;

    dm_server_client_->UploadBrowserPublicKey(
        client_id, dm_token, profile_id, request,
        base::BindOnce(&DMServerClientUnitTest::OnUploadJobCompleted,
                       base::Unretained(this), key, run_loop.QuitClosure()));

    run_loop.Run();
  }

 protected:
  static constexpr char default_key[] = "default key";

  std::unique_ptr<DMServerClient> dm_server_client_;
  base::flat_map<std::string, policy::DMServerJobResult> upload_results_;
  enterprise_management::DeviceManagementRequest captured_request_;
  policy::DeviceManagementService::JobConfiguration::JobType captured_job_type_;

 private:
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService service_{&job_creation_handler_};
};

}  // namespace

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_HappyPath) {
  InitializeUploader();
  SetSuccessfulJobReply();

  enterprise_management::DeviceManagementRequest request;
  request.mutable_browser_public_key_upload_request()->set_signature("Signed!");
  SyncUploadBrowserPublicKey("client id", "dm token", "profile_id", request);

  EXPECT_EQ(policy::DeviceManagementService::JobConfiguration::
                TYPE_BROWSER_UPLOAD_PUBLIC_KEY,
            captured_job_type_);
  EXPECT_EQ("Signed!",
            captured_request_.mutable_browser_public_key_upload_request()
                ->signature());

  EXPECT_EQ(upload_results_[default_key].dm_status, policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(upload_results_[default_key].net_error, net::OK);
  EXPECT_TRUE(upload_results_[default_key]
                  .response.has_browser_public_key_upload_response());
}

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_SequentialCalls) {
  InitializeUploader();
  SetSuccessfulJobReply(2);

  // Second job starts when the first is finished:
  SyncUploadBrowserPublicKey("client id", "dm token", "profile id",
                             enterprise_management::DeviceManagementRequest(),
                             "nice key");
  SyncUploadBrowserPublicKey("client id", "dm token", "profile id",
                             enterprise_management::DeviceManagementRequest(),
                             "nasty key");

  EXPECT_EQ(upload_results_["nice key"].dm_status, policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(upload_results_["nice key"].net_error, net::OK);
  EXPECT_TRUE(upload_results_["nice key"]
                  .response.has_browser_public_key_upload_response());

  EXPECT_EQ(upload_results_["nasty key"].dm_status, policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(upload_results_["nasty key"].net_error, net::OK);
  EXPECT_TRUE(upload_results_["nasty key"]
                  .response.has_browser_public_key_upload_response());
}

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_ParallelCalls) {
  InitializeUploader();
  SetSuccessfulJobReply(2);

  // The first job is still active when we create the second job:
  base::RunLoop nasty_run_loop;
  base::RunLoop nice_run_loop;

  dm_server_client_->UploadBrowserPublicKey(
      "client id", "dm token", "profile id",
      enterprise_management::DeviceManagementRequest(),
      base::BindOnce(&DMServerClientUnitTest::OnUploadJobCompleted,
                     base::Unretained(this), "nasty key",
                     nasty_run_loop.QuitClosure()));
  dm_server_client_->UploadBrowserPublicKey(
      "client id", "dm token", "profile id",
      enterprise_management::DeviceManagementRequest(),
      base::BindOnce(&DMServerClientUnitTest::OnUploadJobCompleted,
                     base::Unretained(this), "nice key",
                     nice_run_loop.QuitClosure()));

  nasty_run_loop.Run();
  nice_run_loop.Run();

  EXPECT_EQ(upload_results_["nice key"].dm_status, policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(upload_results_["nice key"].net_error, net::OK);
  EXPECT_TRUE(upload_results_["nice key"]
                  .response.has_browser_public_key_upload_response());

  EXPECT_EQ(upload_results_["nasty key"].dm_status, policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(upload_results_["nasty key"].net_error, net::OK);
  EXPECT_TRUE(upload_results_["nasty key"]
                  .response.has_browser_public_key_upload_response());
}

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_NetFailed_Logged) {
  InitializeUploader();
  SetFailedJobReply(net::ERR_FAILED, policy::DeviceManagementService::kSuccess);

  SyncUploadBrowserPublicKey("client id", "dm token", "profile id",
                             enterprise_management::DeviceManagementRequest());

  EXPECT_EQ(upload_results_[default_key].net_error, net::ERR_FAILED);
  EXPECT_EQ(upload_results_[default_key].dm_status,
            /* HTTP failed. */ policy::DM_STATUS_REQUEST_FAILED);
  EXPECT_FALSE(upload_results_[default_key]
                   .response.has_browser_public_key_upload_response());
}

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_ServerFailed) {
  InitializeUploader();
  SetFailedJobReply(net::OK,
                    policy::DeviceManagementService::kDeviceIdConflict);

  SyncUploadBrowserPublicKey("client id", "dm token", "profile id",
                             enterprise_management::DeviceManagementRequest());

  EXPECT_EQ(upload_results_[default_key].net_error, net::OK);
  EXPECT_EQ(upload_results_[default_key].dm_status,
            policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT);
  EXPECT_FALSE(upload_results_[default_key]
                   .response.has_browser_public_key_upload_response());
}

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_NoClientId) {
  InitializeUploader();

  std::optional<std::string> client_id = std::nullopt;
  SyncUploadBrowserPublicKey(client_id.value_or(""), "dm token", "profile id",
                             enterprise_management::DeviceManagementRequest());

  EXPECT_EQ(upload_results_[default_key].dm_status,
            policy::DM_STATUS_REQUEST_INVALID);
  EXPECT_EQ(upload_results_[default_key].net_error, net::OK);
  EXPECT_FALSE(upload_results_[default_key]
                   .response.has_browser_public_key_upload_response());
}

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_NoDmToken) {
  InitializeUploader();

  enterprise_management::DeviceManagementRequest request;
  SyncUploadBrowserPublicKey("client id", /* DM token*/ "", "profile_id",
                             request);

  EXPECT_EQ(upload_results_[default_key].dm_status,
            policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID);
  EXPECT_EQ(upload_results_[default_key].net_error, net::OK);
  EXPECT_FALSE(upload_results_[default_key]
                   .response.has_browser_public_key_upload_response());
}

TEST_F(DMServerClientUnitTest, UploadBrowserPublicKey_NullProfileId_Succeeds) {
  InitializeUploader();
  SetSuccessfulJobReply();

  enterprise_management::DeviceManagementRequest request;
  request.mutable_browser_public_key_upload_request()->set_signature("Signed!");
  SyncUploadBrowserPublicKey("client id", "dm token", std::nullopt, request);

  EXPECT_EQ(policy::DeviceManagementService::JobConfiguration::
                TYPE_BROWSER_UPLOAD_PUBLIC_KEY,
            captured_job_type_);
  EXPECT_EQ("Signed!",
            captured_request_.mutable_browser_public_key_upload_request()
                ->signature());

  EXPECT_EQ(upload_results_[default_key].dm_status, policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(upload_results_[default_key].net_error, net::OK);
  EXPECT_TRUE(upload_results_[default_key]
                  .response.has_browser_public_key_upload_response());
}

}  // namespace enterprise_attestation
