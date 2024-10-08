// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/scanner_provider.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/features.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_scanner";
constexpr base::TimeDelta kTimeout = base::Seconds(30);

void OnServerResponseOrErrorReceived(
    ScannerProvider::ScannerProtoResponseCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    std::move(callback).Run(nullptr, std::move(manta_status));
    return;
  }

  DCHECK(manta_response != nullptr);

  // TODO: b/363101024 - Parse ScannerResponse from manta_response.
  std::unique_ptr<proto::ScannerResponse> scanner_response =
      std::make_unique<proto::ScannerResponse>();

  std::move(callback).Run(std::move(scanner_response), std::move(manta_status));
}

}  // namespace

ScannerProvider::ScannerProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

ScannerProvider::~ScannerProvider() = default;

void ScannerProvider::Call(
    ScannerProvider::ScannerProtoResponseCallback done_callback) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_SCANNER);

  // TODO: b/363101024 - Populate annotation tag.
  RequestInternal(
      GURL{GetProviderEndpoint(features::IsScannerUseProdServerEnabled())},
      kOauthConsumerName, /*annotation_tag=*/MISSING_TRAFFIC_ANNOTATION,
      request, MantaMetricType::kScanner,
      base::BindOnce(&OnServerResponseOrErrorReceived,
                     std::move(done_callback)),
      kTimeout);
}

}  // namespace manta
