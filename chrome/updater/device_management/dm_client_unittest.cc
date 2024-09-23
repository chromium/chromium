// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_client.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/device_management/dm_message.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/test/unit_test_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/update_client/network.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::RunClosure;

namespace updater {
namespace {

class TestTokenService
    : public device_management_storage::TokenServiceInterface {
 public:
  TestTokenService(const std::string& enrollment_token,
                   const std::string& dm_token)
      : enrollment_token_(enrollment_token), dm_token_(dm_token) {}
  ~TestTokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return "test-device-id"; }

  bool IsEnrollmentMandatory() const override { return false; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    enrollment_token_ = enrollment_token;
    return true;
  }

  bool DeleteEnrollmentToken() override { return StoreEnrollmentToken(""); }

  std::string GetEnrollmentToken() const override { return enrollment_token_; }

  bool StoreDmToken(const std::string& dm_token) override {
    dm_token_ = dm_token;
    return true;
  }

  bool DeleteDmToken() override {
    dm_token_.clear();
    return true;
  }

  std::string GetDmToken() const override { return dm_token_; }

 private:
  std::string enrollment_token_;
  std::string dm_token_;
};

class TestConfigurator : public DMClient::Configurator {
 public:
  explicit TestConfigurator(const GURL& url);
  ~TestConfigurator() override = default;

  GURL GetDMServerUrl() const override { return server_url_; }

  std::string GetAgentParameter() const override {
    return "Updater-Test-Agent";
  }

  std::string GetPlatformParameter() const override { return "Test-Platform"; }

  std::unique_ptr<update_client::NetworkFetcher> CreateNetworkFetcher()
      const override {
    return network_fetcher_factory_->Create();
  }

 private:
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  const GURL server_url_;
};

TestConfigurator::TestConfigurator(const GURL& url)
    : network_fetcher_factory_(base::MakeRefCounted<NetworkFetcherFactory>(
          PolicyServiceProxyConfiguration::Get(
              test::CreateTestPolicyService()))),
      server_url_(url) {}

class DMRequestCallbackHandler
    : public base::RefCountedThreadSafe<DMRequestCallbackHandler> {
 public:
  DMRequestCallbackHandler() = default;

  enum class PublicKeyType {
    kNone,
    kTestKey1,
    kTestKey2,
  };

  MOCK_METHOD0(PostRequestCompleted, void(void));

  void CreateStorage(bool init_dm_token, bool init_cache_info) {
    ASSERT_TRUE(storage_dir_.CreateUniqueTempDir());
    constexpr char kEnrollmentToken[] = "TestEnrollmentToken";
    constexpr char kDmToken[] = "test-dm-token";
    storage_ =
        CreateDMStorage(storage_dir_.GetPath(),
                        std::make_unique<TestTokenService>(
                            kEnrollmentToken, init_dm_token ? kDmToken : ""));

    if (init_cache_info) {
      ASSERT_TRUE(storage_->CanPersistPolicies());
      std::unique_ptr<::enterprise_management::DeviceManagementResponse>
          dm_response = GetDefaultTestingPolicyFetchDMResponse(
              /*first_request=*/true, /*rotate_to_new_key=*/false,
              DMPolicyBuilderForTesting::SigningOption::kSignNormally);
      std::unique_ptr<device_management_storage::CachedPolicyInfo> info =
          storage_->GetCachedPolicyInfo();
      std::vector<PolicyValidationResult> validation_results;
      DMPolicyMap policies = ParsePolicyFetchResponse(
          dm_response->SerializeAsString(), *info.get(), storage_->GetDmToken(),
          storage_->GetDeviceID(), validation_results);
      ASSERT_FALSE(policies.empty());
      ASSERT_TRUE(storage_->PersistPolicies(policies));
      ASSERT_TRUE(validation_results.empty());
    }
  }

  void SetExpectedHttpStatus(net::HttpStatusCode expected_http_status) {
    expected_http_status_ = expected_http_status;
  }
  void SetExpectedRequestResult(DMClient::RequestResult expected_result) {
    expected_result_ = expected_result;
  }

  void SetExpectedPublicKey(PublicKeyType expect_key_type) {
    expected_public_key_type_ = expect_key_type;
  }

  scoped_refptr<device_management_storage::DMStorage> GetStorage() {
    return storage_;
  }

 protected:
  virtual ~DMRequestCallbackHandler() = default;

  base::ScopedTempDir storage_dir_;
  scoped_refptr<device_management_storage::DMStorage> storage_;

  net::HttpStatusCode expected_http_status_ = net::HTTP_OK;
  DMClient::RequestResult expected_result_ = DMClient::RequestResult::kSuccess;
  PublicKeyType expected_public_key_type_ = PublicKeyType::kNone;

 private:
  friend class base::RefCountedThreadSafe<DMRequestCallbackHandler>;
};

class DMRegisterRequestCallbackHandler : public DMRequestCallbackHandler {
 public:
  explicit DMRegisterRequestCallbackHandler(bool expect_registered)
      : expect_registered_(expect_registered) {}

  void OnRequestComplete(DMClient::RequestResult result) {
    if (expect_registered_) {
      EXPECT_EQ(result, expected_result_);
      if (result == DMClient::RequestResult::kSuccess ||
          result == DMClient::RequestResult::kAlreadyRegistered) {
        EXPECT_EQ(storage_->GetDmToken(), "test-dm-token");
      } else {
        EXPECT_TRUE(storage_->GetDmToken().empty());
      }
    } else {
      // Device should be unregistered.
      EXPECT_EQ(result, DMClient::RequestResult::kDeregistered);
      EXPECT_TRUE(storage_->IsDeviceDeregistered());
    }
    PostRequestCompleted();
  }

 private:
  ~DMRegisterRequestCallbackHandler() override = default;

  const bool expect_registered_;

  friend class base::RefCountedThreadSafe<DMRegisterRequestCallbackHandler>;
};

class DMPolicyFetchRequestCallbackHandler : public DMRequestCallbackHandler {
 public:
  void AppendExpectedValidationResult(const PolicyValidationResult& result) {
    expected_validation_results_.push_back(result);
  }

  void OnRequestComplete(
      DMClient::RequestResult result,
      const std::vector<PolicyValidationResult>& validation_results) {
    EXPECT_EQ(result, expected_result_);

    if (expected_http_status_ != net::HTTP_OK ||
        expected_result_ == DMClient::RequestResult::kNoDMToken) {
      PostRequestCompleted();
      return;
    }

    std::unique_ptr<device_management_storage::CachedPolicyInfo> info =
        storage_->GetCachedPolicyInfo();
    switch (expected_public_key_type_) {
      case PublicKeyType::kTestKey1:
        EXPECT_EQ(info->public_key(), GetTestKey1()->GetPublicKeyString());
        break;
      case PublicKeyType::kTestKey2:
        EXPECT_EQ(info->public_key(), GetTestKey2()->GetPublicKeyString());
        break;
      case PublicKeyType::kNone:
      default:
        EXPECT_TRUE(info->public_key().empty());
        break;
    }

    if (result == DMClient::RequestResult::kSuccess) {
      std::optional<::wireless_android_enterprise_devicemanagement::
                        OmahaSettingsClientProto>
          omaha_settings = GetOmahaPolicySettings(storage_);
      EXPECT_TRUE(omaha_settings);

      // Sample some of the policy values and check they are expected.
      EXPECT_EQ(omaha_settings->proxy_mode(), "pac_script");
      const ::wireless_android_enterprise_devicemanagement::ApplicationSettings&
          chrome_settings = omaha_settings->application_settings()[0];
      EXPECT_EQ(chrome_settings.app_guid(), test::kChromeAppId);
      EXPECT_EQ(chrome_settings.update(),
                ::wireless_android_enterprise_devicemanagement::
                    AUTOMATIC_UPDATES_ONLY);
      EXPECT_EQ(chrome_settings.target_version_prefix(), "81.");
    }

    EXPECT_EQ(expected_validation_results_, validation_results);
    PostRequestCompleted();
  }

 private:
  ~DMPolicyFetchRequestCallbackHandler() override = default;
  friend class base::RefCountedThreadSafe<DMPolicyFetchRequestCallbackHandler>;

  std::vector<PolicyValidationResult> expected_validation_results_;
};

class DMValidationReportRequestCallbackHandler
    : public DMRequestCallbackHandler {
 public:
  void OnRequestComplete(DMClient::RequestResult result) {
    EXPECT_EQ(result, expected_result_);
    PostRequestCompleted();
  }

 private:
  ~DMValidationReportRequestCallbackHandler() override = default;
  friend class base::RefCountedThreadSafe<
      DMValidationReportRequestCallbackHandler>;
};

}  // namespace

class DMClientTest : public ::testing::Test {
 public:
  ~DMClientTest() override = default;

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::string app_type;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "apptype", &app_type));
    EXPECT_EQ(app_type, "Chrome");

    std::string platform;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "platform", &platform));
    EXPECT_EQ(platform, "Test-Platform");

    std::string device_id;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "deviceid", &device_id));
    EXPECT_EQ(device_id, "test-device-id");

    EXPECT_EQ(request.headers.at("Content-Type"), "application/x-protobuf");

    std::string request_type;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "request", &request_type));
    EXPECT_EQ(request_type, GetExpectedRequestType());

    EXPECT_EQ(request.headers.at("Authorization"),
              GetExpectedAuthorizationToken());

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(response_http_status_);
    http_response->set_content_type("application/x-protobuf");
    http_response->set_content(response_body_);
    return http_response;
  }

  void StartTestServerWithResponse(net::HttpStatusCode http_status,
                                   const std::string& body) {
    response_http_status_ = http_status;
    response_body_ = body;
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &DMClientTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  virtual std::string GetExpectedRequestType() const = 0;
  virtual std::string GetExpectedAuthorizationToken() const = 0;

  net::EmbeddedTestServer test_server_;
  net::HttpStatusCode response_http_status_ = net::HTTP_OK;
  std::string response_body_;

  base::test::TaskEnvironment task_environment_;
};

class DMRegisterClientTest : public DMClientTest {
 public:
  void PostRequest() {
    DMClient::RegisterDevice(
        std::make_unique<TestConfigurator>(test_server_.GetURL("/dm_api")),
        callback_handler_->GetStorage(),
        base::BindOnce(&DMRegisterRequestCallbackHandler::OnRequestComplete,
                       callback_handler_));
  }

  std::string GetExpectedAuthorizationToken() const override {
    return "GoogleEnrollmentToken token=TestEnrollmentToken";
  }

  std::string GetExpectedRequestType() const override {
    return "register_policy_agent";
  }

  std::string GetDefaultResponse() const {
    auto dm_response =
        std::make_unique<enterprise_management::DeviceManagementResponse>();
    dm_response->mutable_register_response()->set_device_management_token(
        "test-dm-token");
    return dm_response->SerializeAsString();
  }

  scoped_refptr<DMRegisterRequestCallbackHandler> callback_handler_;
};

class DMPolicyFetchClientTest : public DMClientTest {
 public:
  void PostRequest() {
    DMClient::FetchPolicy(
        std::make_unique<TestConfigurator>(test_server_.GetURL("/dm_api")),
        callback_handler_->GetStorage(),
        base::BindOnce(&DMPolicyFetchRequestCallbackHandler::OnRequestComplete,
                       callback_handler_));
  }

  std::string GetExpectedAuthorizationToken() const override {
    return "GoogleDMToken token=test-dm-token";
  }

  std::string GetExpectedRequestType() const override { return "policy"; }

  scoped_refptr<DMPolicyFetchRequestCallbackHandler> callback_handler_;
};

class DMPolicyValidationReportClientTest : public DMClientTest {
 public:
  void PostRequest(const PolicyValidationResult& validation_result) {
    DMClient::ReportPolicyValidationErrors(
        std::make_unique<TestConfigurator>(test_server_.GetURL("/dm_api")),
        callback_handler_->GetStorage(), validation_result,
        base::BindOnce(
            &DMValidationReportRequestCallbackHandler::OnRequestComplete,
            callback_handler_));
  }

  std::string GetExpectedAuthorizationToken() const override {
    return "GoogleDMToken token=test-dm-token";
  }

  std::string GetExpectedRequestType() const override {
    return "policy_validation_report";
  }

  scoped_refptr<DMValidationReportRequestCallbackHandler> callback_handler_;
};

TEST_F(DMRegisterClientTest, Success) {
  callback_handler_ =
      base::MakeRefCounted<DMRegisterRequestCallbackHandler>(true);
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  StartTestServerWithResponse(net::HTTP_OK, GetDefaultResponse());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));
  PostRequest();
  run_loop.Run();
}

TEST_F(DMRegisterClientTest, Deregister) {
  callback_handler_ =
      base::MakeRefCounted<DMRegisterRequestCallbackHandler>(false);
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  StartTestServerWithResponse(net::HTTP_GONE, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMRegisterClientTest, DeregisterWithDeletion) {
  callback_handler_ =
      base::MakeRefCounted<DMRegisterRequestCallbackHandler>(true);
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kNoDMToken);

  enterprise_management::DeviceManagementResponse response;
  response.add_error_detail(
      enterprise_management::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN);

  StartTestServerWithResponse(net::HTTP_GONE, response.SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMRegisterClientTest, BadRequest) {
  callback_handler_ =
      base::MakeRefCounted<DMRegisterRequestCallbackHandler>(true);
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kTestKey1);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kHttpError);
  StartTestServerWithResponse(net::HTTP_BAD_REQUEST, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMRegisterClientTest, AlreadyRegistered) {
  callback_handler_ =
      base::MakeRefCounted<DMRegisterRequestCallbackHandler>(true);
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kAlreadyRegistered);
  StartTestServerWithResponse(net::HTTP_OK, GetDefaultResponse());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMRegisterClientTest, BadResponseData) {
  callback_handler_ =
      base::MakeRefCounted<DMRegisterRequestCallbackHandler>(true);
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kNone);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);
  StartTestServerWithResponse(net::HTTP_OK, "BadResponseData");

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, NoDMToken) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kTestKey1);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kNoDMToken);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/true,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, FirstRequest) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kTestKey1);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, NoRotateKey) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kTestKey1);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, RotateKey) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kTestKey2);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/true,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, RejectKeyWithBadSignature) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kNone);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);

  PolicyValidationResult expected_validation_result;
  expected_validation_result.policy_type = "google/machine-level-omaha";
  expected_validation_result.status =
      PolicyValidationResult::Status::kValidationBadKeyVerificationSignature;
  callback_handler_->AppendExpectedValidationResult(expected_validation_result);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/true,
          DMPolicyBuilderForTesting::SigningOption::kTamperKeySignature);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, RejectDataWithBadSignature) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kNone);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);
  PolicyValidationResult expected_validation_result;
  expected_validation_result.policy_type = "google/machine-level-omaha";
  expected_validation_result.status =
      PolicyValidationResult::Status::kValidationBadSignature;
  callback_handler_->AppendExpectedValidationResult(expected_validation_result);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kTamperDataSignature);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, Deregister) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kDeregistered);
  callback_handler_->SetExpectedHttpStatus(net::HTTP_GONE);

  StartTestServerWithResponse(net::HTTP_GONE, "" /* response body */);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();

  EXPECT_TRUE(callback_handler_->GetStorage()->IsDeviceDeregistered());
}

TEST_F(DMPolicyFetchClientTest, DeregisterWithDeletion) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kNoDMToken);
  callback_handler_->SetExpectedHttpStatus(net::HTTP_GONE);

  enterprise_management::DeviceManagementResponse response;
  response.add_error_detail(
      enterprise_management::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN);

  StartTestServerWithResponse(net::HTTP_GONE, response.SerializeAsString());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();

  EXPECT_TRUE(callback_handler_->GetStorage()->GetDmToken().empty());
}

TEST_F(DMPolicyFetchClientTest, BadResponse) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kNone);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);
  StartTestServerWithResponse(net::HTTP_OK, "Unexpected response data");

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyFetchClientTest, BadRequest) {
  callback_handler_ =
      base::MakeRefCounted<DMPolicyFetchRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedPublicKey(
      DMRequestCallbackHandler::PublicKeyType::kTestKey1);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kHttpError);
  StartTestServerWithResponse(net::HTTP_BAD_REQUEST, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PostRequest();
  run_loop.Run();
}

TEST_F(DMPolicyValidationReportClientTest, Success) {
  callback_handler_ =
      base::MakeRefCounted<DMValidationReportRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  StartTestServerWithResponse(net::HTTP_OK, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PolicyValidationResult validation_result;
  validation_result.policy_type = kGoogleUpdatePolicyType;
  validation_result.policy_token = "TestPolicyToken";
  validation_result.issues.emplace_back(
      "test_policy", PolicyValueValidationIssue::Severity::kError,
      "Policy value out of range.");
  PostRequest(validation_result);
  run_loop.Run();
}

TEST_F(DMPolicyValidationReportClientTest, NoDMToken) {
  callback_handler_ =
      base::MakeRefCounted<DMValidationReportRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kNoDMToken);
  StartTestServerWithResponse(net::HTTP_OK, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  PolicyValidationResult validation_result;
  validation_result.policy_type = kGoogleUpdatePolicyType;
  validation_result.policy_token = "TestPolicyToken";
  validation_result.issues.emplace_back(
      "test_policy", PolicyValueValidationIssue::Severity::kError,
      "Policy value out of range.");
  PostRequest(validation_result);
  run_loop.Run();
}

TEST_F(DMPolicyValidationReportClientTest, NoPayload) {
  callback_handler_ =
      base::MakeRefCounted<DMValidationReportRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kNoPayload);
  StartTestServerWithResponse(net::HTTP_OK, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));
  PostRequest(PolicyValidationResult());
  run_loop.Run();
}

TEST(DMClient, StreamRequestResultEnumValue) {
  {
    std::stringstream output;
    output << DMClient::RequestResult::kSuccess;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kSuccess");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kNoDeviceID;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kNoDeviceID");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kAlreadyRegistered;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kAlreadyRegistered");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kNotManaged;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kNotManaged");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kDeregistered;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kDeregistered");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kNoDMToken;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kNoDMToken");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kFetcherError;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kFetcherError");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kNetworkError;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kNetworkError");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kHttpError;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kHttpError");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kSerializationError;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kSerializationError");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kUnexpectedResponse;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kUnexpectedResponse");
  }
  {
    std::stringstream output;
    output << DMClient::RequestResult::kNoPayload;
    EXPECT_EQ(output.str(), "DMClient::RequestResult::kNoPayload");
  }
}

}  // namespace updater
