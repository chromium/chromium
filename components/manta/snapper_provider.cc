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
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_snapper";
constexpr char kEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";

}  // namespace

SnapperProvider::SnapperProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : BaseProvider(url_loader_factory, identity_manager) {}

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

  // TODO(b:288019728): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  std::unique_ptr<EndpointFetcher> fetcher =
      CreateEndpointFetcher(GURL{kEndpointUrl}, kOauthConsumerName,
                            MISSING_TRAFFIC_ANNOTATION, serialized_request);

  EndpointFetcher* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                    std::move(done_callback),
                                    std::move(fetcher)));
}

}  // namespace manta
