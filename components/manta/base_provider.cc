// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/base_provider.h"

#include "base/containers/fixed_flat_map.h"
#include "base/version_info/channel.h"
#include "components/manta/proto/manta.pb.h"

namespace manta {
namespace {
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/x-protobuf";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr char kAutopushEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";
constexpr char kProdEndpointUrl[] = "https://aratea-pa.googleapis.com/generate";

using manta::proto::ChromeClientInfo;

ChromeClientInfo::Channel ConvertChannel(version_info::Channel channel) {
  static constexpr auto kChannelMap =
      base::MakeFixedFlatMap<version_info::Channel,
                             manta::proto::ChromeClientInfo::Channel>(
          {{version_info::Channel::UNKNOWN, ChromeClientInfo::UNKNOWN},
           {version_info::Channel::CANARY, ChromeClientInfo::CANARY},
           {version_info::Channel::DEV, ChromeClientInfo::DEV},
           {version_info::Channel::BETA, ChromeClientInfo::BETA},
           {version_info::Channel::STABLE, ChromeClientInfo::STABLE}});
  auto iter = kChannelMap.find(channel);
  if (iter == kChannelMap.end()) {
    return manta::proto::ChromeClientInfo::UNKNOWN;
  }
  return iter->second;
}
}  // namespace

std::string GetProviderEndpoint(bool use_prod) {
  return use_prod ? kProdEndpointUrl : kAutopushEndpointUrl;
}

BaseProvider::BaseProvider() = default;

BaseProvider::BaseProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : BaseProvider(url_loader_factory, identity_manager, ProviderParams()) {}

BaseProvider::BaseProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : url_loader_factory_(url_loader_factory),
      provider_params_(provider_params) {
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
    MantaProtoResponseCallback done_callback,
    const base::TimeDelta timeout) {
  if (!provider_params_.use_api_key &&
      !identity_manager_observation_.IsObserving()) {
    std::move(done_callback)
        .Run(nullptr, {MantaStatusCode::kNoIdentityManager});
    return;
  }

  // Add additional info to the request proto.
  auto* client_info = request.mutable_client_info();
  client_info->set_client_type(manta::proto::ClientInfo::CHROME);

  if (!provider_params_.chrome_version.empty()) {
    client_info->mutable_chrome_client_info()->set_chrome_version(
        provider_params_.chrome_version);
  }

  client_info->mutable_chrome_client_info()->set_chrome_channel(
      ConvertChannel(provider_params_.chrome_channel));

  if (!provider_params_.locale.empty()) {
    client_info->mutable_chrome_client_info()->set_locale(
        provider_params_.locale);
  }

  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  base::Time start_time = base::Time::Now();

  if (provider_params_.use_api_key) {
    std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcherForDemoMode(
        url, annotation_tag, serialized_request, timeout);
    EndpointFetcher* const fetcher_ptr = fetcher.get();
    fetcher_ptr->PerformRequest(
        base::BindOnce(&OnEndpointFetcherComplete, std::move(done_callback),
                       start_time, metric_type, std::move(fetcher)),
        nullptr);
  } else {
    std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcher(
        url, oauth_consumer_name, annotation_tag, serialized_request, timeout);
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
    const std::string& post_data,
    const base::TimeDelta timeout) {
  CHECK(identity_manager_observation_.IsObserving());
  const std::vector<std::string>& scopes{kOAuthScope};
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/oauth_consumer_name,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*scopes=*/scopes,
      /*timeout=*/timeout,
      /*post_data=*/post_data,
      /*annotation_tag=*/annotation_tag,
      /*identity_manager=*/identity_manager_observation_.GetSource(),
      /*consent_level=*/signin::ConsentLevel::kSignin);
}

std::unique_ptr<EndpointFetcher> BaseProvider::CreateEndpointFetcherForDemoMode(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const std::string& post_data,
    const base::TimeDelta timeout) {
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout=*/timeout,
      /*post_data=*/post_data,
      /*headers=*/std::vector<std::string>(),
      /*cors_exempt_headers=*/std::vector<std::string>(),
      /*annotation_tag=*/annotation_tag,
      // ChromeOS always uses the stable channel API key
      version_info::Channel::STABLE);
}

}  // namespace manta
