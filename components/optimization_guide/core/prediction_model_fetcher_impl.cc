// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_fetcher_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace optimization_guide {

PredictionModelFetcherImpl::PredictionModelFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& optimization_guide_service_get_models_url)
    : optimization_guide_service_get_models_url_(
          net::AppendOrReplaceQueryParameter(
              optimization_guide_service_get_models_url,
              "key",
              optimization_guide::features::
                  GetOptimizationGuideServiceAPIKey())),
      url_loader_factory_(url_loader_factory) {
  CHECK(optimization_guide_service_get_models_url_.SchemeIs(url::kHttpsScheme));
}

PredictionModelFetcherImpl::~PredictionModelFetcherImpl() = default;

bool PredictionModelFetcherImpl::FetchOptimizationGuideServiceModels(
    const std::vector<proto::ModelInfo>& models_request_info,
    proto::RequestContext request_context,
    const std::string& locale,
    ModelsFetchedCallback models_fetched_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (url_loader_)
    return false;

  // If there are no models to request, do not make a GetModelsRequest.
  if (models_request_info.empty()) {
    std::move(models_fetched_callback).Run(std::nullopt);
    return false;
  }

  pending_models_request_ =
      std::make_unique<optimization_guide::proto::GetModelsRequest>();

  pending_models_request_->set_request_context(request_context);
  pending_models_request_->set_locale(locale);
  *pending_models_request_->mutable_origin_info() =
      optimization_guide::GetClientOriginInfo();

  for (const auto& model_request_info : models_request_info)
    *pending_models_request_->add_requested_models() = model_request_info;

  std::string serialized_request;
  pending_models_request_->SerializeToString(&serialized_request);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("optimization_guide_model",
                                          R"(
        semantics {
          sender: "Optimization Guide"
          description:
            "Requests the updated set of machine learning models from the "
            "Optimization Guide Service that are applicable to the current "
            "client version."
          trigger:
            "Requested at the beginning of each session if there are features "
            "enabled by the current client version that require machine "
            "learning models."
          data: "A list of models supported by the client."
          destination: GOOGLE_OWNED_SERVICE
        }
         policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          chrome_policy {
            ComponentUpdatesEnabled {
              policy_options {mode: MANDATORY}
              ComponentUpdatesEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = optimization_guide_service_get_models_url_;

  // POST request for providing the GetModelsRequest proto to the remote
  // Optimization Guide Service.
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request),
      // This is always InIncognito::kNo as the OptimizationGuideKeyedService is
      // not enabled on incognito sessions and is rechecked before each fetch.
      variations::InIncognito::kNo, variations::SignedIn::kNo,
      traffic_annotation);

  url_loader_->AttachStringForUpload(serialized_request,
                                     "application/x-protobuf");

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PredictionModelFetcherImpl::OnURLLoadComplete,
                     base::Unretained(this)));

  models_fetched_callback_ = std::move(models_fetched_callback);
  return true;
}

void PredictionModelFetcherImpl::HandleResponse(
    const std::string& get_models_response_data,
    int net_status,
    int response_code) {
  std::unique_ptr<optimization_guide::proto::GetModelsResponse>
      get_models_response =
          std::make_unique<optimization_guide::proto::GetModelsResponse>();

  if (response_code >= 0 && response_code <= net::HTTP_VERSION_NOT_SUPPORTED) {
    UMA_HISTOGRAM_ENUMERATION(
        "OptimizationGuide.PredictionModelFetcher."
        "GetModelsResponse.Status",
        static_cast<net::HttpStatusCode>(response_code),
        net::HTTP_VERSION_NOT_SUPPORTED);
  }
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.PredictionModelFetcher."
      "GetModelsResponse.NetErrorCode",
      -net_status);

  for (const auto& model_info : pending_models_request_->requested_models()) {
    if (response_code >= 0 &&
        response_code <= net::HTTP_VERSION_NOT_SUPPORTED) {
      base::UmaHistogramEnumeration(
          "OptimizationGuide.PredictionModelFetcher."
          "GetModelsResponse.Status." +
              optimization_guide::GetStringNameForOptimizationTarget(
                  model_info.optimization_target()),
          static_cast<net::HttpStatusCode>(response_code),
          net::HTTP_VERSION_NOT_SUPPORTED);
    }
    // Net error codes are negative but histogram enums must be positive.
    base::UmaHistogramSparse(
        "OptimizationGuide.PredictionModelFetcher."
        "GetModelsResponse.NetErrorCode." +
            optimization_guide::GetStringNameForOptimizationTarget(
                model_info.optimization_target()),
        -net_status);
  }

  if (net_status == net::OK && response_code == net::HTTP_OK &&
      get_models_response->ParseFromString(get_models_response_data)) {
    std::move(models_fetched_callback_).Run(std::move(get_models_response));
  } else {
    std::move(models_fetched_callback_).Run(std::nullopt);
  }
}

void PredictionModelFetcherImpl::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  HandleResponse(response_body ? *response_body : "", url_loader_->NetError(),
                 response_code);
  url_loader_.reset();
}

}  // namespace optimization_guide
