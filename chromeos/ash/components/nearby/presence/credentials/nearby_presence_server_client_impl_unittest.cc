// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow_impl.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client.h"
#include "chromeos/ash/components/nearby/presence/proto/list_shared_credentials_rpc.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/rpc_resources.pb.h"
#include "chromeos/ash/components/nearby/presence/proto/update_device_rpc.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kGet[] = "GET";
const char kPost[] = "POST";
const char kPatch[] = "PATCH";
const char kAccessToken[] = "access_token";
const char kDeviceIdPath[] = "users/me/devices/deviceid";
const char kEmail[] = "test@gmail.com";
const char kLocalDeviceDusi[] = "12345";
const int64_t kId1 = 123;
const char kTestGoogleApisUrl[] = "https://nearbypresence-pa.googleapis.com";

class FakeNearbyApiCallFlow : public ash::nearby::NearbyApiCallFlow {
 public:
  FakeNearbyApiCallFlow() = default;
  ~FakeNearbyApiCallFlow() override = default;
  FakeNearbyApiCallFlow(FakeNearbyApiCallFlow&) = delete;
  FakeNearbyApiCallFlow& operator=(FakeNearbyApiCallFlow&) = delete;

  void StartPostRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kPost;
    request_url_ = request_url;
    serialized_request_ = serialized_request;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }

  void StartPatchRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kPatch;
    request_url_ = request_url;
    serialized_request_ = serialized_request;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }

  void StartGetRequest(
      const GURL& request_url,
      const QueryParameters& request_as_query_parameters,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kGet;
    request_url_ = request_url;
    request_as_query_parameters_ = request_as_query_parameters;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }

  void SetPartialNetworkTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override {
    // Do nothing
  }

  std::string http_method_;
  GURL request_url_;
  std::string serialized_request_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  ResultCallback result_callback_;
  ErrorCallback error_callback_;
  QueryParameters request_as_query_parameters_;
};

// Return the values associated with |key|, or fail the test if |key| isn't in
// |query_parameters|
std::vector<std::string> ExpectQueryStringValues(
    const ash::nearby::NearbyApiCallFlow::QueryParameters& query_parameters,
    const std::string& key) {
  std::vector<std::string> values;
  for (const std::pair<std::string, std::string>& pair : query_parameters) {
    if (pair.first == key) {
      values.push_back(pair.second);
    }
  }
  EXPECT_TRUE(values.size() > 0);
  return values;
}

// Callback that should never be invoked.
template <class T>
void NotCalled(T type) {
  EXPECT_TRUE(false);
}

// Callback that should never be invoked.
template <class T>
void NotCalledConstRef(const T& type) {
  EXPECT_TRUE(false);
}

}  // namespace

namespace ash::nearby::presence {

class NearbyPresenceServerClientImplTest : public testing::Test {
 protected:
  NearbyPresenceServerClientImplTest() {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  void SetUp() override {
    identity_test_environment_.MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSync);

    std::unique_ptr<FakeNearbyApiCallFlow> api_call_flow =
        std::make_unique<FakeNearbyApiCallFlow>();
    api_call_flow_ = api_call_flow.get();

    client_ = NearbyPresenceServerClientImpl::Factory::Create(
        std::move(api_call_flow), identity_test_environment_.identity_manager(),
        shared_factory_);
  }

  const std::string& http_method() { return api_call_flow_->http_method_; }

  const GURL& request_url() { return api_call_flow_->request_url_; }

  const std::string& serialized_request() {
    return api_call_flow_->serialized_request_;
  }

  const NearbyApiCallFlow::QueryParameters& request_as_query_parameters() {
    return api_call_flow_->request_as_query_parameters_;
  }

  // Returns |response_proto| as the result to the current API request.
  void FinishApiCallFlow(const google::protobuf::MessageLite* response_proto) {
    std::move(api_call_flow_->result_callback_)
        .Run(response_proto->SerializeAsString());
  }

  // Returns |serialized_proto| as the result to the current API request.
  void FinishApiCallFlowRaw(const std::string& serialized_proto) {
    std::move(api_call_flow_->result_callback_).Run(serialized_proto);
  }

  // Ends the current API request with |error|.
  void FailApiCallFlow(NearbyHttpError error) {
    std::move(api_call_flow_->error_callback_).Run(error);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  raw_ptr<FakeNearbyApiCallFlow, DanglingUntriaged> api_call_flow_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<NearbyPresenceServerClient> client_;
};

TEST_F(NearbyPresenceServerClientImplTest, UpdateDeviceSuccess) {
  base::test::TestFuture<const ash::nearby::proto::UpdateDeviceResponse&>
      future;
  ash::nearby::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);
  client_->UpdateDevice(request_proto, future.GetCallback(),
                        base::BindOnce(&NotCalled<NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());
  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), GURL(std::string(kTestGoogleApisUrl) +
                                "/v1/presence/" + std::string(kDeviceIdPath)));

  ash::nearby::proto::UpdateDeviceRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request()));
  EXPECT_EQ(kDeviceIdPath, expected_request.device().name());

  ash::nearby::proto::UpdateDeviceResponse response_proto;
  ash::nearby::proto::Device& device = *response_proto.mutable_device();

  device.set_name(kDeviceIdPath);
  FinishApiCallFlow(&response_proto);

  // Check that the result received in callback is the same as the response.
  ash::nearby::proto::UpdateDeviceResponse result_proto = future.Take();
  EXPECT_EQ(kDeviceIdPath, result_proto.device().name());
}

TEST_F(NearbyPresenceServerClientImplTest, UpdateDeviceFailure) {
  ash::nearby::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(kDeviceIdPath);

  base::test::TestFuture<NearbyHttpError> future;
  client_->UpdateDevice(
      request,
      base::BindOnce(
          &NotCalledConstRef<ash::nearby::proto::UpdateDeviceResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), GURL(std::string(kTestGoogleApisUrl) +
                                "/v1/presence/" + std::string(kDeviceIdPath)));

  FailApiCallFlow(NearbyHttpError::kInternalServerError);
  EXPECT_EQ(NearbyHttpError::kInternalServerError, future.Get());
}

TEST_F(NearbyPresenceServerClientImplTest, ListSharedCredentialsSuccess) {
  base::test::TestFuture<
      const ash::nearby::proto::ListSharedCredentialsResponse&>
      future;
  ash::nearby::proto::ListSharedCredentialsRequest request_proto;
  request_proto.set_dusi(kLocalDeviceDusi);
  request_proto.set_identity_type(
      ash::nearby::proto::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP);

  client_->ListSharedCredentials(request_proto, future.GetCallback(),
                                 base::BindOnce(&NotCalled<NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kGet, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/presence/" +
                               std::string(kLocalDeviceDusi) + "/1" +
                               "/listSharedCredentials");

  EXPECT_EQ(
      std::vector<std::string>{base::NumberToString(1)},
      ExpectQueryStringValues(request_as_query_parameters(), "identity_type"));
  EXPECT_EQ(std::vector<std::string>{kLocalDeviceDusi},
            ExpectQueryStringValues(request_as_query_parameters(), "dusi"));

  ash::nearby::proto::ListSharedCredentialsResponse response_proto;
  response_proto.add_shared_credentials();
  response_proto.mutable_shared_credentials(0)->set_id(kId1);
  FinishApiCallFlow(&response_proto);

  ash::nearby::proto::ListSharedCredentialsResponse result_proto =
      future.Take();
  EXPECT_EQ(1, result_proto.shared_credentials_size());
  EXPECT_EQ(kId1, result_proto.shared_credentials(0).id());
}

TEST_F(NearbyPresenceServerClientImplTest, FetchAccessTokenFailure) {
  base::test::TestFuture<NearbyHttpError> future;
  client_->UpdateDevice(
      ash::nearby::proto::UpdateDeviceRequest(),
      base::BindOnce(
          &NotCalledConstRef<ash::nearby::proto::UpdateDeviceResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));

  EXPECT_EQ(NearbyHttpError::kAuthenticationError, future.Get());
}

TEST_F(NearbyPresenceServerClientImplTest, ParseResponseProtoFailure) {
  ash::nearby::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);

  base::test::TestFuture<NearbyHttpError> future;
  client_->UpdateDevice(
      request_proto,
      base::BindOnce(
          &NotCalledConstRef<ash::nearby::proto::UpdateDeviceResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/presence/" +
                               std::string(kDeviceIdPath));

  FinishApiCallFlowRaw("Not a valid serialized response message.");
  EXPECT_EQ(NearbyHttpError::kResponseMalformed, future.Get());
}

TEST_F(NearbyPresenceServerClientImplTest,
       MakeSecondRequestBeforeFirstRequestSucceeds) {
  ash::nearby::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);

  // Make first request.
  base::test::TestFuture<const ash::nearby::proto::UpdateDeviceResponse&>
      future;
  client_->UpdateDevice(request_proto, future.GetCallback(),
                        base::BindOnce(&NotCalled<NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/presence/" +
                               std::string(kDeviceIdPath));

  // With request pending, make second request.
  {
    base::test::TestFuture<NearbyHttpError> future2;
    EXPECT_DCHECK_DEATH(client_->ListSharedCredentials(
        ash::nearby::proto::ListSharedCredentialsRequest(),
        base::BindOnce(&NotCalledConstRef<
                       ash::nearby::proto::ListSharedCredentialsResponse>),
        future2.GetCallback()));
  }

  // Complete first request.
  {
    ash::nearby::proto::UpdateDeviceResponse response_proto;
    response_proto.mutable_device()->set_name(kDeviceIdPath);
    FinishApiCallFlow(&response_proto);
  }

  ash::nearby::proto::UpdateDeviceResponse result_proto = future.Take();
  EXPECT_EQ(kDeviceIdPath, result_proto.device().name());
}

TEST_F(NearbyPresenceServerClientImplTest,
       MakeSecondRequestAfterFirstRequestSucceeds) {
  // Make first request successfully.
  {
    base::test::TestFuture<const ash::nearby::proto::UpdateDeviceResponse&>
        future;
    ash::nearby::proto::UpdateDeviceRequest request_proto;
    request_proto.mutable_device()->set_name(kDeviceIdPath);

    client_->UpdateDevice(request_proto, future.GetCallback(),
                          base::BindOnce(&NotCalled<NearbyHttpError>));
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kAccessToken, base::Time::Max());

    EXPECT_EQ(kPatch, http_method());
    EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/presence/" +
                                 std::string(kDeviceIdPath));

    ash::nearby::proto::UpdateDeviceResponse response_proto;
    response_proto.mutable_device()->set_name(kDeviceIdPath);
    FinishApiCallFlow(&response_proto);
    ash::nearby::proto::UpdateDeviceResponse result_proto = future.Take();
    EXPECT_EQ(kDeviceIdPath, result_proto.device().name());
  }

  // Second request fails.
  {
    base::test::TestFuture<NearbyHttpError> future;
    EXPECT_DCHECK_DEATH(client_->ListSharedCredentials(
        ash::nearby::proto::ListSharedCredentialsRequest(),
        base::BindOnce(&NotCalledConstRef<
                       ash::nearby::proto::ListSharedCredentialsResponse>),
        future.GetCallback()));
  }
}

TEST_F(NearbyPresenceServerClientImplTest, GetAccessTokenUsed) {
  EXPECT_TRUE(client_->GetAccessTokenUsed().empty());

  base::test::TestFuture<const ash::nearby::proto::UpdateDeviceResponse&>
      future;
  ash::nearby::proto::UpdateDeviceRequest request_proto;
  request_proto.mutable_device()->set_name(kDeviceIdPath);

  client_->UpdateDevice(request_proto, future.GetCallback(),
                        base::BindOnce(&NotCalled<NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kPatch, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/presence/" +
                               std::string(kDeviceIdPath));

  EXPECT_EQ(kAccessToken, client_->GetAccessTokenUsed());
}

}  // namespace ash::nearby::presence
