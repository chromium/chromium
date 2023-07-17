// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_server_client_impl.h"
#include "chromeos/ash/components/nearby/presence/proto/list_public_certificates_rpc.pb.h"
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
const char kPageToken1[] = "pagetoken1";
const char kPageToken2[] = "pagetoken2";
const char kPublicKey1[] = "publickey1";
const char kSecretId1[] = "secretid1";
const char kSecretId2[] = "secretid2";
const char kSecretId1Encoded[] = "c2VjcmV0aWQx";
const char kSecretId2Encoded[] = "c2VjcmV0aWQy";
const char kSecretKey1[] = "secretkey1";
const char kTestGoogleApisUrl[] = "https://nearbypresence-pa.googleapis.com";
const int32_t kNanos1 = 123123123;
const int32_t kNanos2 = 321321321;
const int32_t kPageSize1 = 1000;
const int64_t kSeconds1 = 1594392109;
const int64_t kSeconds2 = 1623336109;

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
  raw_ptr<FakeNearbyApiCallFlow, ExperimentalAsh> api_call_flow_;
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

TEST_F(NearbyPresenceServerClientImplTest, ListPublicCertificatesSuccess) {
  base::test::TestFuture<
      const ash::nearby::proto::ListPublicCertificatesResponse&>
      future;
  ash::nearby::proto::ListPublicCertificatesRequest request_proto;
  request_proto.set_parent(kDeviceIdPath);
  request_proto.set_page_size(kPageSize1);
  request_proto.set_page_token(kPageToken1);
  request_proto.add_secret_ids();
  request_proto.set_secret_ids(0, kSecretId1);
  request_proto.add_secret_ids();
  request_proto.set_secret_ids(1, kSecretId2);

  client_->ListPublicCertificates(request_proto, future.GetCallback(),
                                  base::BindOnce(&NotCalled<NearbyHttpError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());

  EXPECT_EQ(kGet, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/presence/" +
                               std::string(kDeviceIdPath) +
                               "/publicCertificates");

  EXPECT_EQ(
      std::vector<std::string>{base::NumberToString(kPageSize1)},
      ExpectQueryStringValues(request_as_query_parameters(), "page_size"));
  EXPECT_EQ(
      std::vector<std::string>{kPageToken1},
      ExpectQueryStringValues(request_as_query_parameters(), "page_token"));
  EXPECT_EQ(
      (std::vector<std::string>{kSecretId1Encoded, kSecretId2Encoded}),
      ExpectQueryStringValues(request_as_query_parameters(), "secret_ids"));

  ash::nearby::proto::ListPublicCertificatesResponse response_proto;
  response_proto.set_next_page_token(kPageToken2);
  response_proto.add_public_certificates();
  response_proto.mutable_public_certificates(0)->set_secret_id(kSecretId1);
  response_proto.mutable_public_certificates(0)->set_secret_key(kSecretKey1);
  response_proto.mutable_public_certificates(0)->set_public_key(kPublicKey1);
  response_proto.mutable_public_certificates(0)
      ->mutable_start_time()
      ->set_seconds(kSeconds1);
  response_proto.mutable_public_certificates(0)
      ->mutable_start_time()
      ->set_nanos(kNanos1);
  response_proto.mutable_public_certificates(0)
      ->mutable_end_time()
      ->set_seconds(kSeconds2);
  response_proto.mutable_public_certificates(0)->mutable_end_time()->set_nanos(
      kNanos2);
  FinishApiCallFlow(&response_proto);

  ash::nearby::proto::ListPublicCertificatesResponse result_proto =
      future.Take();
  EXPECT_EQ(kPageToken2, result_proto.next_page_token());
  EXPECT_EQ(1, result_proto.public_certificates_size());
  EXPECT_EQ(kSecretId1, result_proto.public_certificates(0).secret_id());
  EXPECT_EQ(kSecretKey1, result_proto.public_certificates(0).secret_key());
  EXPECT_EQ(kSeconds1,
            result_proto.public_certificates(0).start_time().seconds());
  EXPECT_EQ(kNanos1, result_proto.public_certificates(0).start_time().nanos());
  EXPECT_EQ(kSeconds2,
            result_proto.public_certificates(0).end_time().seconds());
  EXPECT_EQ(kNanos2, result_proto.public_certificates(0).end_time().nanos());
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
    EXPECT_DCHECK_DEATH(client_->ListPublicCertificates(
        ash::nearby::proto::ListPublicCertificatesRequest(),
        base::BindOnce(&NotCalledConstRef<
                       ash::nearby::proto::ListPublicCertificatesResponse>),
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
    EXPECT_DCHECK_DEATH(client_->ListPublicCertificates(
        ash::nearby::proto::ListPublicCertificatesRequest(),
        base::BindOnce(&NotCalledConstRef<
                       ash::nearby::proto::ListPublicCertificatesResponse>),
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
