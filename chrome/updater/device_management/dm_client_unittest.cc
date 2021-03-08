// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_client.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_cached_policy_info.h"
#include "chrome/updater/device_management/dm_message.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/policy_manager.h"
#include "chrome/updater/unittest_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/update_client/network.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/updater/win/net/network.h"
#elif defined(OS_MAC)
#include "chrome/updater/mac/net/network.h"
#endif  // OS_WIN

using base::test::RunClosure;

namespace updater {

namespace {

class TestTokenService : public TokenServiceInterface {
 public:
  TestTokenService(const std::string& enrollment_token,
                   const std::string& dm_token)
      : enrollment_token_(enrollment_token), dm_token_(dm_token) {}
  ~TestTokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return "test-device-id"; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    enrollment_token_ = enrollment_token;
    return true;
  }

  std::string GetEnrollmentToken() const override { return enrollment_token_; }

  bool StoreDmToken(const std::string& dm_token) override {
    dm_token_ = dm_token;
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

  std::string GetDMServerUrl() const override { return server_url_; }

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
  const std::string server_url_;
};

TestConfigurator::TestConfigurator(const GURL& url)
    : network_fetcher_factory_(base::MakeRefCounted<NetworkFetcherFactory>()),
      server_url_(url.spec()) {}

class DMRequestCallbackHandler
    : public base::RefCountedThreadSafe<DMRequestCallbackHandler> {
 public:
  DMRequestCallbackHandler() = default;
  void OnRegisterRequestComplete(DMClient::RequestResult result) {
    EXPECT_EQ(result, expected_result_);
    if (result == DMClient::RequestResult::kSuccess ||
        result == DMClient::RequestResult::kAleadyRegistered) {
      EXPECT_EQ(storage_->GetDmToken(), "test-dm-token");
    } else {
      EXPECT_TRUE(storage_->GetDmToken().empty());
    }
    PostRequestCompleted();
  }

  void OnPolicyFetchRequestComplete(DMClient::RequestResult result) {
    EXPECT_EQ(result, expected_result_);
    if (expected_http_status_ == net::HTTP_GONE)
      EXPECT_TRUE(storage_->IsDeviceDeregistered());

    if (expected_http_status_ != net::HTTP_OK) {
      PostRequestCompleted();
      return;
    }

    std::unique_ptr<CachedPolicyInfo> info = storage_->GetCachedPolicyInfo();
    EXPECT_FALSE(info->public_key().empty());
    if (expect_new_public_key_)
      EXPECT_EQ(info->public_key(), GetTestKey2()->GetPublicKeyString());
    else
      EXPECT_EQ(info->public_key(), GetTestKey1()->GetPublicKeyString());

    if (result == DMClient::RequestResult::kSuccess) {
      std::unique_ptr<PolicyManagerInterface> policy_manager =
          storage_->GetOmahaPolicyManager();
      EXPECT_NE(policy_manager, nullptr);

      // Sample some of the policy values and check they are expected.
      EXPECT_TRUE(policy_manager->IsManaged());
      std::string proxy_mode;
      EXPECT_TRUE(policy_manager->GetProxyMode(&proxy_mode));
      EXPECT_EQ(proxy_mode, "test_proxy_mode");
      int update_policy = 0;
      EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppUpdates(
          kChromeAppId, &update_policy));
      EXPECT_EQ(update_policy, kPolicyAutomaticUpdatesOnly);
      std::string target_version_prefix;
      EXPECT_TRUE(policy_manager->GetTargetVersionPrefix(
          kChromeAppId, &target_version_prefix));
      EXPECT_EQ(target_version_prefix, "81.");
    }

    PostRequestCompleted();
  }

  void OnDeregisterRequestComplete(DMClient::RequestResult result) {
    EXPECT_EQ(result, DMClient::RequestResult::kSuccess);
    EXPECT_TRUE(storage_->IsDeviceDeregistered());
    PostRequestCompleted();
  }

  MOCK_METHOD0(PostRequestCompleted, void(void));

  void CreateStorage(bool init_dm_token, bool init_cache_info) {
    ASSERT_TRUE(storage_dir_.CreateUniqueTempDir());
    constexpr char kEnrollmentToken[] = "TestEnrollmentToken";
    constexpr char kDmToken[] = "test-dm-token";
    storage_ = base::MakeRefCounted<DMStorage>(
        storage_dir_.GetPath(),
        std::make_unique<TestTokenService>(kEnrollmentToken,
                                           init_dm_token ? kDmToken : ""));

    if (init_cache_info) {
      std::unique_ptr<::enterprise_management::DeviceManagementResponse>
          dm_response = GetDefaultTestingPolicyFetchDMResponse(
              /*first_request=*/true, /*rotate_to_new_key=*/false,
              DMPolicyBuilderForTesting::SigningOption::kSignNormally);
      std::unique_ptr<CachedPolicyInfo> info = storage_->GetCachedPolicyInfo();
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

  void SetExpectNewPublicKey(bool expect_new_key) {
    expect_new_public_key_ = expect_new_key;
  }

  scoped_refptr<DMStorage> GetStorage() { return storage_; }

 private:
  friend class base::RefCountedThreadSafe<DMRequestCallbackHandler>;
  ~DMRequestCallbackHandler() = default;

  base::ScopedTempDir storage_dir_;
  scoped_refptr<DMStorage> storage_;

  net::HttpStatusCode expected_http_status_ = net::HTTP_OK;
  DMClient::RequestResult expected_result_ = DMClient::RequestResult::kSuccess;
  bool expect_new_public_key_ = false;
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
    if (request_type == "register_policy_agent") {
      std::string authorization = request.headers.at("Authorization");
      EXPECT_EQ(authorization,
                "GoogleEnrollmentToken token=TestEnrollmentToken");
    } else if (request_type == "policy") {
      std::string authorization = request.headers.at("Authorization");
      EXPECT_EQ(authorization, "GoogleDMToken token=test-dm-token");
    } else {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_BAD_REQUEST);
      return http_response;
    }

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

  std::string GetDefaultDeviceRegisterResponse() const {
    auto dm_response =
        std::make_unique<enterprise_management::DeviceManagementResponse>();
    dm_response->mutable_register_response()->set_device_management_token(
        "test-dm-token");
    return dm_response->SerializeAsString();
  }

  void CreateDMClient() {
    const GURL url = test_server_.GetURL("/dm_api");
    auto test_config = std::make_unique<TestConfigurator>(url);
    client_ = std::make_unique<DMClient>(std::move(test_config),
                                         callback_handler_->GetStorage());
  }

  net::EmbeddedTestServer test_server_;
  net::HttpStatusCode response_http_status_ = net::HTTP_OK;
  std::string response_body_;

  std::unique_ptr<DMClient> client_;
  scoped_refptr<DMRequestCallbackHandler> callback_handler_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DMClientTest, PostRegisterRequest_Success) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  StartTestServerWithResponse(net::HTTP_OK, GetDefaultDeviceRegisterResponse());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostRegisterRequest(base::BindOnce(
      &DMRequestCallbackHandler::OnRegisterRequestComplete, callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_Deregister) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  StartTestServerWithResponse(net::HTTP_GONE, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostRegisterRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnDeregisterRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_BadRequest) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kHttpError);
  StartTestServerWithResponse(net::HTTP_BAD_REQUEST, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostRegisterRequest(base::BindOnce(
      &DMRequestCallbackHandler::OnRegisterRequestComplete, callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_AlreadyRegistered) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kAleadyRegistered);
  StartTestServerWithResponse(net::HTTP_OK, GetDefaultDeviceRegisterResponse());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostRegisterRequest(base::BindOnce(
      &DMRequestCallbackHandler::OnRegisterRequestComplete, callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_BadResponseData) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/false,
                                   /*init_cache_info=*/false);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);
  StartTestServerWithResponse(net::HTTP_OK, "BadResponseData");

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostRegisterRequest(base::BindOnce(
      &DMRequestCallbackHandler::OnRegisterRequestComplete, callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_FirstRequest) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/false);
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

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_NoRotateKey) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
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

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_RotateKey) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  callback_handler_->SetExpectNewPublicKey(true);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/true,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_RejectKeyWithBadSignature) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);
  callback_handler_->SetExpectNewPublicKey(false);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/true,
          DMPolicyBuilderForTesting::SigningOption::kTamperKeySignature);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_RejectDataWithBadSignature) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);
  callback_handler_->SetExpectNewPublicKey(false);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kTamperDataSignature);
  StartTestServerWithResponse(net::HTTP_OK, dm_response->SerializeAsString());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_Deregister) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kSuccess);
  callback_handler_->SetExpectedHttpStatus(net::HTTP_GONE);

  StartTestServerWithResponse(net::HTTP_GONE, "" /* response body */);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_BadResponse) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kUnexpectedResponse);
  StartTestServerWithResponse(net::HTTP_OK, "Unexpected response data");

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

TEST_F(DMClientTest, PostPolicyFetch_BadRequest) {
  callback_handler_ = base::MakeRefCounted<DMRequestCallbackHandler>();
  callback_handler_->CreateStorage(/*init_dm_token=*/true,
                                   /*init_cache_info=*/true);
  callback_handler_->SetExpectedRequestResult(
      DMClient::RequestResult::kHttpError);
  StartTestServerWithResponse(net::HTTP_BAD_REQUEST, "" /* response body */);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*callback_handler_, PostRequestCompleted())
      .WillOnce(RunClosure(quit_closure));

  CreateDMClient();
  client_->PostPolicyFetchRequest(
      base::BindOnce(&DMRequestCallbackHandler::OnPolicyFetchRequestComplete,
                     callback_handler_));
  run_loop.Run();
}

}  // namespace updater
