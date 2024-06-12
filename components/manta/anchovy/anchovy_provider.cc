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
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/proto/manta.pb.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_orca";
constexpr char kEndpointUrl[] =
    "https://autopush-aratea-pa.sandbox.googleapis.com/generate";

proto::Request ComposeRequest(const anchovy::ImageDescriptionRequest& request) {
  proto::Request request_proto;
  request_proto.set_feature_name(
      proto::FeatureName::ACCESSIBILITY_IMAGE_DESCRIPTION);

  auto* input_data = request_proto.add_input_data();
  input_data->mutable_image()->set_serialized_bytes(
      std::string(request.image_bytes->begin(), request.image_bytes->end()));

  return request_proto;
}

// TODO(b/340320912 - francisjp): Improve parsing logic to chose best caption.
void HandleResponseOrError(MantaGenericCallback callback,
                           std::unique_ptr<proto::Response> manta_response,
                           MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    CHECK(manta_response == nullptr);
    std::move(callback).Run(base::Value::Dict(), std::move(manta_status));
    return;
  }

  CHECK(manta_response != nullptr);

  // An empty response is still an acceptable response.
  if (manta_response->output_data_size() < 1 ||
      !manta_response->output_data(0).has_text()) {
    std::move(callback).Run(base::Value::Dict(),
                            {MantaStatusCode::kOk, std::string()});
    return;
  }

  base::Value::List results;
  for (const auto& data : manta_response->output_data()) {
    if (data.has_text()) {
      results.Append(base::Value::Dict()
                         .Set("text", data.text())
                         .Set("score", data.score()));
    }
  }

  std::move(callback).Run(
      base::Value::Dict().Set("results", std::move(results)),
      std::move(manta_status));
}

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
  auto proto_request = ComposeRequest(request);

  RequestInternal(
      GURL(kEndpointUrl), kOauthConsumerName, traffic_annotation, proto_request,
      MantaMetricType::kAnchovy,
      base::BindOnce(&HandleResponseOrError, std::move(done_callback)));
}

}  // namespace manta
