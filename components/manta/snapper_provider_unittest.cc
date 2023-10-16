// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/snapper_provider.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/manta/manta_status.h"
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

class FakeSnapperProvider : public SnapperProvider {
 public:
  FakeSnapperProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : SnapperProvider(std::move(test_url_loader_factory), identity_manager) {}

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

class SnapperProviderTest : public testing::Test {
 public:
  SnapperProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  SnapperProviderTest(const SnapperProviderTest&) = delete;
  SnapperProviderTest& operator=(const SnapperProviderTest&) = delete;

  ~SnapperProviderTest() override = default;

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

  std::unique_ptr<FakeSnapperProvider> CreateSnapperProvider() {
    return std::make_unique<FakeSnapperProvider>(
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

// Test POST data is correctly passed from SnapperProvider to EndpointFetcher.
TEST_F(SnapperProviderTest, SimpleRequestPayload) {
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  manta::proto::Image& image = *output_data.mutable_image();
  image.set_serialized_bytes("foo");
  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSnapperProvider> snapper_provider =
      CreateSnapperProvider();
  auto quit_closure = task_environment_.QuitClosure();

  snapper_provider->Call(
      manta::proto::Request(),
      base::BindLambdaForTesting(
          [&quit_closure](std::unique_ptr<manta::proto::Response> response,
                          MantaStatus manta_status) {
            ASSERT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            ASSERT_TRUE(response);
            ASSERT_EQ(1, response->output_data_size());
            ASSERT_TRUE(response->output_data(0).has_image());
            ASSERT_TRUE(
                response->output_data(0).image().has_serialized_bytes());
            EXPECT_EQ("foo",
                      response->output_data(0).image().serialized_bytes());
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(SnapperProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  std::unique_ptr<FakeSnapperProvider> snapper_provider =
      CreateSnapperProvider();

  identity_test_env_.reset();

  snapper_provider->Call(
      manta::proto::Request(),
      base::BindLambdaForTesting(
          [quit_closure = task_environment_.QuitClosure()](
              std::unique_ptr<manta::proto::Response> response,
              MantaStatus manta_status) {
            ASSERT_FALSE(response);
            ASSERT_EQ(MantaStatusCode::kNoIdentityManager,
                      manta_status.status_code);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

}  // namespace manta
