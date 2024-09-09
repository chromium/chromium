// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_provider.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/anchovy/anchovy_proto_helper.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_orca";
constexpr base::TimeDelta kTimeout = base::Seconds(30);

}  // namespace

AnchovyProvider::AnchovyProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

AnchovyProvider::~AnchovyProvider() = default;

void AnchovyProvider::GetImageDescription(
    anchovy::ImageDescriptionRequest& request,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    MantaGenericCallback done_callback) {
  auto proto_request = anchovy::AnchovyProtoHelper::ComposeRequest(request);

  RequestInternal(
      GURL(GetProviderEndpoint(/*use_prod=*/false)), kOauthConsumerName,
      traffic_annotation, proto_request, MantaMetricType::kAnchovy,
      base::BindOnce(
          &anchovy::AnchovyProtoHelper::HandleImageDescriptionResponse,
          std::move(done_callback)),
      kTimeout);
}

}  // namespace manta
