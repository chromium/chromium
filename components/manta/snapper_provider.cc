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
#include "components/manta/base_provider.h"
#include "components/manta/features.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_snapper";

}  // namespace

SnapperProvider::SnapperProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_demo_mode,
    const std::string& chrome_version)
    : BaseProvider(url_loader_factory,
                   identity_manager,
                   is_demo_mode,
                   chrome_version) {}

SnapperProvider::SnapperProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : SnapperProvider(url_loader_factory,
                      identity_manager,
                      false,
                      std::string()) {}

SnapperProvider::~SnapperProvider() = default;

void SnapperProvider::Call(manta::proto::Request& request,
                           net::NetworkTrafficAnnotationTag traffic_annotation,
                           MantaProtoResponseCallback done_callback) {
  if (!is_demo_mode_ && !identity_manager_observation_.IsObserving()) {
    std::move(done_callback)
        .Run(nullptr, {MantaStatusCode::kNoIdentityManager});
    return;
  }
  auto* client_info = request.mutable_client_info();
  client_info->set_client_type(manta::proto::ClientInfo::CHROME);
  client_info->mutable_chrome_client_info()->set_chrome_version(
      chrome_version_);

  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  if (is_demo_mode_) {
    std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcherForDemoMode(
        GURL{GetProviderEndpoint(features::IsSeaPenUseProdServerEnabled())},
        traffic_annotation, serialized_request);
    EndpointFetcher* const fetcher_ptr = fetcher.get();
    fetcher_ptr->PerformRequest(
        base::BindOnce(&OnEndpointFetcherComplete, std::move(done_callback),
                       std::move(fetcher)),
        nullptr);
  } else {
    std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcher(
        GURL{GetProviderEndpoint(features::IsSeaPenUseProdServerEnabled())},
        kOauthConsumerName, traffic_annotation, serialized_request);

    EndpointFetcher* const fetcher_ptr = fetcher.get();
    fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                      std::move(done_callback),
                                      std::move(fetcher)));
  }
}

}  // namespace manta
