// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/orca_provider.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/rpc_status.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

namespace {

constexpr base::TimeDelta kMockTimeout = base::Seconds(100);
constexpr char kMockOAuthConsumerName[] = "mock_oauth_consumer_name";
constexpr char kMockScope[] = "mock_scope";
constexpr char kMockEndpoint[] = "https://my-endpoint.com";
constexpr char kHttpMethod[] = "POST";
constexpr char kMockContentType[] = "mock_content_type";
constexpr char kEmail[] = "mock_email@gmail.com";

}  // namespace

// TODO(b:288019728): refactor into some reused test_util
class FakeOrcaProvider : public OrcaProvider {
 public:
  FakeOrcaProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : OrcaProvider(std::move(test_url_loader_factory), identity_manager) {}

 private:
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::vector<std::string>& scopes,
      const std::string& post_data) override {
    CHECK(identity_manager_observation_.IsObserving());
    return std::make_unique<EndpointFetcher>(
        /*url_loader_factory=*/url_loader_factory_,
        /*oauth_consumer_name=*/kMockOAuthConsumerName,
        /*url=*/GURL{kMockEndpoint},
        /*http_method=*/kHttpMethod, /*content_type=*/kMockContentType,
        /*scopes=*/std::vector<std::string>{kMockScope},
        /*timeout_ms=*/kMockTimeout.InMilliseconds(), /*post_data=*/post_data,
        /*annotation_tag=*/TRAFFIC_ANNOTATION_FOR_TESTS,
        /*identity_manager=*/identity_manager_observation_.GetSource(),
        /*consent_level=*/signin::ConsentLevel::kSync);
  }
};

class OrcaProviderTest : public testing::Test {
 public:
  OrcaProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  OrcaProviderTest(const OrcaProviderTest&) = delete;
  OrcaProviderTest& operator=(const OrcaProviderTest&) = delete;

  ~OrcaProviderTest() override = default;

  void SetUp() override {
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSync);
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  }

  void SetEndpointMockResponse(const GURL& request_url,
                               const std::string& response_data,
                               net::HttpStatusCode response_code,
                               net::Error error) {
    auto head = network::mojom::URLResponseHead::New();
    std::string headers(base::StringPrintf(
        "HTTP/1.1 %d %s\nContent-type: application/x-protobuf\n\n",
        static_cast<int>(response_code), GetHttpReasonPhrase(response_code)));
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head->mime_type = "application/x-protobuf";
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = response_data.size();
    test_url_loader_factory_.AddResponse(request_url, std::move(head),
                                         response_data, status);
  }

  std::unique_ptr<FakeOrcaProvider> CreateOrcaProvider() {
    return std::make_unique<FakeOrcaProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Test OrcaProvider rejects invalid input data. Currently we require the
// input must contain a valid tone.
TEST_F(OrcaProviderTest, PrepareRequestFailure) {
  std::map<std::string, std::string> input = {{"data", "simple post data"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kInvalidInput);
                   quit_closure.Run();
                 }));

  task_environment_.RunUntilQuit();
}

// Test that responses with http_status_code != net::HTTP_OK are captured.
TEST_F(OrcaProviderTest, CaptureUnexcpetedStatusCode) {
  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_BAD_REQUEST, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kBackendFailure);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

TEST_F(OrcaProviderTest, CaptureNetError) {
  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/"",
                          net::HTTP_OK, net::ERR_FAILED);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kNoInternetConnection);
                   quit_closure.Run();
                 }));
  task_environment_.RunUntilQuit();
}

// Test that malformed proto data can be captured with proper error.
TEST_F(OrcaProviderTest, ParseMalformedSerializedProto) {
  std::string post_data = "{invalid proto";

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint}, /*response_data=*/post_data,
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kMalformedResponse);
                   quit_closure.Run();
                 }));

  task_environment_.RunUntilQuit();
}

// Test that an unexpected response with a serialized RpcStatus proto can be
// handled properly.
TEST_F(OrcaProviderTest, ParseRpcStatusFromFailedResponse) {
  proto::RpcStatus rpc_status;
  rpc_status.set_code(3);  // Will be mapped to kInvalidInput.
  rpc_status.set_message("foo");

  proto::RpcLocalizedMessage localize_message;
  localize_message.set_message("bar");
  auto* detail = rpc_status.add_details();
  detail->set_type_url("type.googleapis.com/google.rpc.LocalizedMessage");
  detail->set_value(localize_message.SerializeAsString());

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/rpc_status.SerializeAsString(),
                          net::HTTP_BAD_REQUEST, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kInvalidInput);
                   EXPECT_EQ(manta_status.message, "bar");
                   quit_closure.Run();
                 }));

  task_environment_.RunUntilQuit();

  // ErrorInfo.reason can be mapped to kRestrictedCountry and override the manta
  // status code.
  proto::RpcErrorInfo error_info;
  error_info.set_reason("RESTRICTED_COUNTRY");
  error_info.set_domain("aratea-pa.googleapis.com");
  detail = rpc_status.add_details();
  detail->set_type_url("type.googleapis.com/google.rpc.ErrorInfo");
  detail->set_value(error_info.SerializeAsString());

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/rpc_status.SerializeAsString(),
                          net::HTTP_BAD_REQUEST, net::OK);

  orca_provider->Call(
      input, base::BindLambdaForTesting(
                 [quit_closure = task_environment_.QuitClosure()](
                     base::Value::Dict response, MantaStatus manta_status) {
                   EXPECT_EQ(manta_status.status_code,
                             MantaStatusCode::kRestrictedCountry);
                   EXPECT_EQ(manta_status.message, "bar");
                   quit_closure.Run();
                 }));

  task_environment_.RunUntilQuit();
}

// Test a successful response can be parsed as base::Value::Dict.
TEST_F(OrcaProviderTest, ParseSuccessfulResponse) {
  proto::Response response;
  proto::OutputData& output_data = *response.add_output_data();
  output_data.set_text("foo");

  std::map<std::string, std::string> input = {{"data", "simple post data"},
                                              {"tone", "SHORTEN"}};
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  SetEndpointMockResponse(GURL{kMockEndpoint},
                          /*response_data=*/response.SerializeAsString(),
                          net::HTTP_OK, net::OK);

  orca_provider->Call(
      input,
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              base::Value::Dict response, MantaStatus manta_status) {
            EXPECT_EQ(manta_status.status_code, MantaStatusCode::kOk);

            EXPECT_TRUE(response.contains("outputData"));

            const auto* output_data_list = response.FindList("outputData");
            EXPECT_EQ(output_data_list->size(), 1u);

            const base::Value& front_element = output_data_list->front();
            EXPECT_TRUE(front_element.is_dict());
            EXPECT_EQ(*(front_element.GetDict().FindString("text")), "foo");

            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(OrcaProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  std::unique_ptr<FakeOrcaProvider> orca_provider = CreateOrcaProvider();

  identity_test_env_.reset();

  orca_provider->Call(
      {}, base::BindLambdaForTesting(
              [quit_closure = task_environment_.QuitClosure()](
                  base::Value::Dict dict, MantaStatus manta_status) {
                ASSERT_TRUE(dict.empty());
                ASSERT_EQ(MantaStatusCode::kNoIdentityManager,
                          manta_status.status_code);
                quit_closure.Run();
              }));
  task_environment_.RunUntilQuit();
}

}  // namespace manta
