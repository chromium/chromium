// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_provider.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/anchovy/anchovy_proto_helper.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/base_provider.h"
#include "components/manta/features.h"
#include "components/manta/manta_service_callbacks.h"

namespace manta {

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(30);

}  // namespace

AnchovyProvider::AnchovyProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

AnchovyProvider::~AnchovyProvider() = default;

void AnchovyProvider::GetImageDescriptions(
    const anchovy::ImageDescriptionBatchRequest& request,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    MantaGenericCallback done_callback) {
  CHECK(!request.images.empty());
  if (request.images.size() > anchovy::kMaxBatchSize) {
    // Fail fast without serializing image bytes or issuing an HTTP request
    // because the server rejects batches exceeding `kMaxBatchSize` images.
    std::move(done_callback)
        .Run(base::DictValue(),
             {MantaStatusCode::kInvalidInput, std::string()});
    return;
  }

  // Serialize image bytes synchronously before returning while caller-owned
  // buffers referenced by `request.images` are guaranteed alive.
  proto::Request proto_request =
      anchovy::AnchovyProtoHelper::ComposeRequest(request);

  RequestInternal(
      GURL(GetProviderEndpoint(features::IsAnchovyUseProdServerEnabled())),
      traffic_annotation, proto_request, MantaMetricType::kAnchovy,
      base::BindOnce(
          &anchovy::AnchovyProtoHelper::HandleImageDescriptionsResponse,
          std::move(done_callback)),
      kTimeout);
}

}  // namespace manta
