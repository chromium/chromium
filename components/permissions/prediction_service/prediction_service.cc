// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_service.h"

#include <cmath>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/permissions/features.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr base::TimeDelta kURLLookupTimeout = base::Seconds(2);

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("permission_predictions", R"(
    semantics {
      sender: "Web Permission Perdictions"
      description:
        "A request to the Web Permission Predictions Service. The service will "
        "attempt to predict the likelihood that the user would grant this "
        "permission. Based on this prediction Chrome might decide to present "
        "the user with a different UI; a less intrusive one."
      trigger:
        "A permission prompt is about to be shown to the user, and the user "
        "has opted into Safe Browsing."
      data:
        "User stats helpful for attempting to predict the user's likelihood "
        "of granting the permission: the permission type, the presence of a "
        "user gesture, the user's OS, average deny/grant/ignore/dismiss rates "
        "and total prompts shown, both for the specific permission type and "
        "overall for all permission types."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "This can be disabled by disabling Safe Browsing by going to "
        "Settings and then to the Security sub-menu."
      chrome_policy {
        SafeBrowsingProtectionLevel {
          SafeBrowsingProtectionLevel: 0
        }
      }
    })");
}

}  // namespace

namespace permissions {

PredictionService::PredictionService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}
PredictionService::~PredictionService() = default;

void PredictionService::StartLookup(const PredictionRequestFeatures& entity,
                                    LookupRequestCallback request_callback,
                                    LookupResponseCallback response_callback) {
  auto request = GetResourceRequest();
  auto proto_request = GetPredictionRequestProto(entity);
  std::string request_data;

  proto_request->SerializeToString(&request_data);

  SendRequestInternal(std::move(request), request_data, entity,
                      std::move(response_callback));

  if (request_callback)
    std::move(request_callback).Run(std::move(proto_request), std::string());
}

// static
const GURL PredictionService::GetPredictionServiceUrl(
    bool recalculate_for_testing) {
  static base::NoDestructor<GURL> default_prediction_service_url{
      kDefaultPredictionServiceUrl};
  static base::NoDestructor<GURL> command_line_url_override{
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kDefaultPredictionServiceUrlSwitchKey)};

  // To facilitate tests that want to exercise various url building logic,
  // reinitialize the static variables if this flag is set.
  if (recalculate_for_testing) {
    *command_line_url_override =
        GURL(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kDefaultPredictionServiceUrlSwitchKey));
  }

  if (command_line_url_override->is_valid())
    return *command_line_url_override;

  return *default_prediction_service_url;
}

std::unique_ptr<network::ResourceRequest>
PredictionService::GetResourceRequest() {
  auto request = std::make_unique<network::ResourceRequest>();

  request->url =
      prediction_service_url_override_.is_empty()
          ? GetPredictionServiceUrl(recalculate_service_url_every_time)
          : prediction_service_url_override_;
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->method = "POST";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return request;
}

void PredictionService::SendRequestInternal(
    std::unique_ptr<network::ResourceRequest> request,
    const std::string& request_data,
    const PredictionRequestFeatures& entity,
    LookupResponseCallback response_callback) {
  std::unique_ptr<network::SimpleURLLoader> owned_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       GetTrafficAnnotationTag());
  owned_loader->AttachStringForUpload(request_data, "application/x-protobuf");

  owned_loader->SetTimeoutDuration(kURLLookupTimeout);
  owned_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PredictionService::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), entity, owned_loader.get(),
                     base::TimeTicks::Now()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);

  pending_requests_[std::move(owned_loader)] = std::move(response_callback);
}

void PredictionService::OnURLLoaderComplete(
    const PredictionRequestFeatures& entity,
    network::SimpleURLLoader* loader,
    base::TimeTicks request_start_time,
    std::unique_ptr<std::string> response_body) {
  for (auto& request : pending_requests_) {
    if (request.first.get() == loader) {
      auto prediction_response =
          CreatePredictionsResponse(loader, response_body.get());

      if (request.second) {
        std::optional<GeneratePredictionsResponse> response;
        if (prediction_response == nullptr) {
          response = std::nullopt;
        } else {
          response = *prediction_response;
        }
        bool lookup_success = prediction_response != nullptr;
        std::move(request.second)
            .Run(lookup_success, /*Response from cache=*/false, response);
      }

      pending_requests_.erase(request.first);
      return;
    }
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected loader callback.";
}

std::unique_ptr<GeneratePredictionsResponse>
PredictionService::CreatePredictionsResponse(network::SimpleURLLoader* loader,
                                             const std::string* response_body) {
  if (!response_body || loader->NetError() != net::OK ||
      loader->ResponseInfo()->headers->response_code() != net::HTTP_OK) {
    return nullptr;
  }

  auto predictions_response = std::make_unique<GeneratePredictionsResponse>();
  if (!predictions_response->ParseFromString(*response_body)) {
    return nullptr;
  }

  return predictions_response;
}

}  // namespace permissions
