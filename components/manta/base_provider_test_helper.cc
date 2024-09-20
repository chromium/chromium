// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/base_provider_test_helper.h"

#include "base/strings/stringprintf.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

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

FakeBaseProvider::FakeBaseProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : BaseProvider(url_loader_factory, identity_manager) {}

FakeBaseProvider::~FakeBaseProvider() = default;

void FakeBaseProvider::RequestInternal(
    const GURL& url,
    const std::string& oauth_consumer_name,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    manta::proto::Request& request,
    const MantaMetricType metric_type,
    MantaProtoResponseCallback done_callback,
    const base::TimeDelta timeout) {
  if (!identity_manager_observation_.IsObserving()) {
    std::move(done_callback)
        .Run(nullptr, {MantaStatusCode::kNoIdentityManager});
    return;
  }

  auto fetcher = std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/kMockOAuthConsumerName,
      /*url=*/GURL{kMockEndpoint},
      /*http_method=*/kHttpMethod, /*content_type=*/kMockContentType,
      /*scopes=*/std::vector<std::string>{kMockScope},
      /*timeout=*/kMockTimeout, /*post_data=*/request.SerializeAsString(),
      /*annotation_tag=*/TRAFFIC_ANNOTATION_FOR_TESTS,
      /*identity_manager=*/identity_manager_observation_.GetSource(),
      /*consent_level=*/signin::ConsentLevel::kSync);

  EndpointFetcher* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                    std::move(done_callback), base::Time::Now(),
                                    metric_type, std::move(fetcher)));
}

BaseProviderTest::BaseProviderTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

BaseProviderTest::~BaseProviderTest() = default;

void BaseProviderTest::SetUp() {
  identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
  identity_test_env_->MakePrimaryAccountAvailable(kEmail,
                                                  signin::ConsentLevel::kSync);
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
}

void BaseProviderTest::SetEndpointMockResponse(
    const GURL& request_url,
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

}  // namespace manta
