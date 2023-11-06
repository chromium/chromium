// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/access_token_helper.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace optimization_guide {

namespace {

constexpr char kGoogleAPITypeName[] = "type.googleapis.com/";

std::string_view GetStringNameForModelExecutionFeature(
    proto::ModelExecutionFeature feature) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      return "WallpaperSearch";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      return "TabOrganization";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      return "Compose";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      return "Unknown";
      // Must be in sync with the ModelExecutionFeature variant in
      // optimization/histograms.xml for metric recording.
  }
}

void RecordRequestStatusHistogram(proto::ModelExecutionFeature feature,
                                  FetcherRequestStatus status) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecutionFetcher.RequestStatus.",
                    GetStringNameForModelExecutionFeature(feature)}),
      status);
}

}  // namespace

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

ModelExecutionFetcher::ModelExecutionFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& optimization_guide_service_url,
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_service_url_(optimization_guide_service_url),
      url_loader_factory_(url_loader_factory),
      optimization_guide_logger_(optimization_guide_logger) {
  CHECK(optimization_guide_service_url_.SchemeIs(url::kHttpsScheme));
}

ModelExecutionFetcher::~ModelExecutionFetcher() {
  if (active_url_loader_) {
    DCHECK_NE(proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED,
              model_execution_feature_);
    RecordRequestStatusHistogram(model_execution_feature_,
                                 FetcherRequestStatus::kRequestCanceled);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure)));
  }
}

void ModelExecutionFetcher::ExecuteModel(
    proto::ModelExecutionFeature feature,
    signin::IdentityManager* identity_manager,
    const std::set<std::string>& oauth_scopes,
    const google::protobuf::MessageLite& request_metadata,
    ModelExecuteResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED,
            feature);

  if (active_url_loader_) {
    RecordRequestStatusHistogram(feature, FetcherRequestStatus::kFetcherBusy);
    std::move(callback).Run(base::unexpected(
        OptimizationGuideModelExecutionError::FromModelExecutionError(
            ModelExecutionError::kGenericFailure)));
    return;
  }

  fetch_start_time_ = base::TimeTicks::Now();
  model_execution_feature_ = feature;
  model_execution_callback_ = std::move(callback);

  proto::ExecuteRequest execute_request;
  execute_request.set_feature(feature);
  proto::Any* any_metadata = execute_request.mutable_request_metadata();
  any_metadata->set_type_url(
      base::StrCat({kGoogleAPITypeName, request_metadata.GetTypeName()}));
  request_metadata.SerializeToString(any_metadata->mutable_value());
  std::string serialized_request;
  execute_request.SerializeToString(&serialized_request);

  RequestAccessToken(
      identity_manager, oauth_scopes,
      base::BindOnce(&ModelExecutionFetcher::OnAccessTokenReceived,
                     weak_ptr_factory_.GetWeakPtr(), serialized_request));
}

void ModelExecutionFetcher::OnAccessTokenReceived(
    const std::string& serialized_request,
    const std::string& access_token) {
  if (access_token.empty()) {
    RecordRequestStatusHistogram(model_execution_feature_,
                                 FetcherRequestStatus::kUserNotSignedIn);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kPermissionDenied)));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  if (!access_token.empty()) {
    PopulateAuthorizationRequestHeader(resource_request.get(), access_token);
  }

  resource_request->url = optimization_guide_service_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  active_url_loader_ = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request),
      // This is always InIncognito::kNo as the server model execution is not
      // enabled on incognito sessions and is rechecked before each fetch.
      variations::InIncognito::kNo, variations::SignedIn::kNo,
      // TODO(crbug/1485313): Update the traffic annotations with more details
      // about the features.
      MISSING_TRAFFIC_ANNOTATION);

  active_url_loader_->AttachStringForUpload(serialized_request,
                                            "application/x-protobuf");
  active_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ModelExecutionFetcher::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModelExecutionFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto net_error = active_url_loader_->NetError();
  int response_code = -1;
  if (active_url_loader_->ResponseInfo() &&
      active_url_loader_->ResponseInfo()->headers) {
    response_code =
        active_url_loader_->ResponseInfo()->headers->response_code();
  }

  // Reset the active URL loader here since actions happening during response
  // handling may start a new fetch.
  active_url_loader_.reset();

  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecutionFetcher.Status",
      static_cast<net::HttpStatusCode>(response_code),
      net::HTTP_VERSION_NOT_SUPPORTED);
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.ModelExecutionFetcher.NetErrorCode", -net_error);

  proto::ExecuteResponse execute_response;

  if (net_error != net::OK || response_code != net::HTTP_OK) {
    RecordRequestStatusHistogram(model_execution_feature_,
                                 FetcherRequestStatus::kResponseError);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromHttpStatusCode(
                static_cast<net::HttpStatusCode>(response_code))));
    return;
  }
  if (!response_body || !execute_response.ParseFromString(*response_body)) {
    RecordRequestStatusHistogram(model_execution_feature_,
                                 FetcherRequestStatus::kResponseError);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure)));
    return;
  }
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecutionFetcher.FetchLatency.",
           GetStringNameForModelExecutionFeature(model_execution_feature_)}),
      base::TimeTicks::Now() - fetch_start_time_);
  RecordRequestStatusHistogram(model_execution_feature_,
                               FetcherRequestStatus::kSuccess);
  // This should be the last call, since the callback could be deleting `this`.
  std::move(model_execution_callback_).Run(base::ok(execute_response));
}

}  // namespace optimization_guide
