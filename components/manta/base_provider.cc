// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/base_provider.h"

namespace manta {
namespace {
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/x-protobuf";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr base::TimeDelta kTimeout = base::Seconds(30);
constexpr char kAutopushEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";
constexpr char kProdEndpointUrl[] = "https://aratea-pa.googleapis.com/generate";

}  // namespace

std::string GetProviderEndpoint(bool use_prod) {
  return use_prod ? kProdEndpointUrl : kAutopushEndpointUrl;
}

BaseProvider::BaseProvider() : use_api_key_(false) {}
BaseProvider::BaseProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool use_api_key,
    const std::string& chrome_version,
    const std::string& locale)
    : url_loader_factory_(url_loader_factory),
      use_api_key_(use_api_key),
      chrome_version_(chrome_version),
      locale_(locale) {
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

BaseProvider::~BaseProvider() = default;

void BaseProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_observation_.IsObservingSource(identity_manager)) {
    identity_manager_observation_.Reset();
  }
}

void BaseProvider::RequestInternal(
    const GURL& url,
    const std::string& oauth_consumer_name,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    manta::proto::Request& request,
    const MantaMetricType metric_type,
    MantaProtoResponseCallback done_callback) {
  if (!use_api_key_ && !identity_manager_observation_.IsObserving()) {
    std::move(done_callback)
        .Run(nullptr, {MantaStatusCode::kNoIdentityManager});
    return;
  }

  // Add additional info to the request proto.
  auto* client_info = request.mutable_client_info();
  client_info->set_client_type(manta::proto::ClientInfo::CHROME);

  if (!chrome_version_.empty()) {
    client_info->mutable_chrome_client_info()->set_chrome_version(
        chrome_version_);
  }

  if (!locale_.empty()) {
    client_info->mutable_chrome_client_info()->set_locale(locale_);
  }

  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  base::Time start_time = base::Time::Now();

  if (use_api_key_) {
    std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcherForDemoMode(
        url, annotation_tag, serialized_request);
    EndpointFetcher* const fetcher_ptr = fetcher.get();
    fetcher_ptr->PerformRequest(
        base::BindOnce(&OnEndpointFetcherComplete, std::move(done_callback),
                       start_time, metric_type, std::move(fetcher)),
        nullptr);
  } else {
    std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcher(
        url, oauth_consumer_name, annotation_tag, serialized_request);
    EndpointFetcher* const fetcher_ptr = fetcher.get();
    fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                      std::move(done_callback), start_time,
                                      metric_type, std::move(fetcher)));
  }
}

std::unique_ptr<EndpointFetcher> BaseProvider::CreateEndpointFetcher(
    const GURL& url,
    const std::string& oauth_consumer_name,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const std::string& post_data) {
  CHECK(identity_manager_observation_.IsObserving());
  const std::vector<std::string>& scopes{kOAuthScope};
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/oauth_consumer_name,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*scopes=*/scopes,
      /*timeout=*/kTimeout,
      /*post_data=*/post_data,
      /*annotation_tag=*/annotation_tag,
      /*identity_manager=*/identity_manager_observation_.GetSource(),
      /*consent_level=*/signin::ConsentLevel::kSignin);
}

std::unique_ptr<EndpointFetcher> BaseProvider::CreateEndpointFetcherForDemoMode(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const std::string& post_data) {
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout=*/kTimeout,
      /*post_data=*/post_data,
      /*headers=*/std::vector<std::string>(),
      /*cors_exempt_headers=*/std::vector<std::string>(),
      /*annotation_tag=*/annotation_tag,
      // ChromeOS always uses the stable channel API key
      /*is_stable_channel=*/true);
}

}  // namespace manta
