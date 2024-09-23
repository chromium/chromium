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
constexpr base::TimeDelta kTimeout = base::Seconds(30);

}  // namespace

SnapperProvider::SnapperProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

SnapperProvider::SnapperProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : BaseProvider(url_loader_factory, identity_manager) {}

SnapperProvider::~SnapperProvider() = default;

void SnapperProvider::Call(manta::proto::Request& request,
                           net::NetworkTrafficAnnotationTag traffic_annotation,
                           MantaProtoResponseCallback done_callback) {
  RequestInternal(
      GURL{GetProviderEndpoint(features::IsSeaPenUseProdServerEnabled())},
      kOauthConsumerName, traffic_annotation, request,
      MantaMetricType::kSnapper, std::move(done_callback), kTimeout);
}

}  // namespace manta
