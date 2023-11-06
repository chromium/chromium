// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/snapper_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_snapper";
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/x-protobuf";
constexpr char kEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr base::TimeDelta kTimeoutMs = base::Seconds(90);

}  // namespace

SnapperProvider::SnapperProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory) {
  CHECK(identity_manager);
  identity_manager_observation_.Observe(identity_manager);
}

SnapperProvider::~SnapperProvider() = default;

void SnapperProvider::Call(const manta::proto::Request& request,
                           MantaProtoResponseCallback done_callback) {
  if (!identity_manager_observation_.IsObserving()) {
    std::move(done_callback)
        .Run(nullptr, {MantaStatusCode::kNoIdentityManager});
    return;
  }
  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcher(
      GURL{kEndpointUrl}, {kOAuthScope}, serialized_request);

  EndpointFetcher* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                    std::move(done_callback),
                                    std::move(fetcher)));
}

void SnapperProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_observation_.IsObservingSource(identity_manager)) {
    identity_manager_observation_.Reset();
  }
}

std::unique_ptr<EndpointFetcher> SnapperProvider::CreateEndpointFetcher(
    const GURL& url,
    const std::vector<std::string>& scopes,
    const std::string& post_data) {
  CHECK(identity_manager_observation_.IsObserving());
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/kOauthConsumerName, /*url=*/url,
      /*http_method=*/kHttpMethod, /*content_type=*/kHttpContentType,
      /*scopes=*/scopes,
      /*timeout_ms=*/kTimeoutMs.InMilliseconds(), /*post_data=*/post_data,
      /*annotation_tag=*/MISSING_TRAFFIC_ANNOTATION,
      /*identity_manager=*/identity_manager_observation_.GetSource(),
      /*consent_level=*/signin::ConsentLevel::kSignin);
}

}  // namespace manta
