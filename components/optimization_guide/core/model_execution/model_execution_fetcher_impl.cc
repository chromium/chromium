// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_fetcher_impl.h"

#include <optional>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/access_token_helper.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_execution_common.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace optimization_guide {

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

ModelExecutionFetcherImpl::ModelExecutionFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& optimization_guide_service_url,
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_service_url_(optimization_guide_service_url),
      url_loader_factory_(url_loader_factory),
      optimization_guide_logger_(optimization_guide_logger) {
  if (!net::IsLocalhost(optimization_guide_service_url_)) {
    CHECK(optimization_guide_service_url_.SchemeIs(url::kHttpsScheme));
  }
}

ModelExecutionFetcherImpl::~ModelExecutionFetcherImpl() {
  if (model_execution_callback_) {
    DCHECK(model_execution_feature_);
    RecordRequestStatusHistogram(*model_execution_feature_,
                                 FetcherRequestStatus::kRequestCanceled);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kCancelled)));
  }
}

void ModelExecutionFetcherImpl::ExecuteModel(
    ModelBasedCapabilityKey feature,
    signin::IdentityManager* identity_manager,
    const google::protobuf::MessageLite& request_metadata,
    std::optional<base::TimeDelta> timeout,
    ModelExecuteResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (model_execution_callback_) {
    RecordRequestStatusHistogram(feature, FetcherRequestStatus::kFetcherBusy);
    std::move(callback).Run(base::unexpected(
        OptimizationGuideModelExecutionError::FromModelExecutionError(
            ModelExecutionError::kGenericFailure)));
    return;
  }

  fetch_start_time_ = base::TimeTicks::Now();
  model_execution_feature_ = feature;
  model_execution_callback_ = std::move(callback);

  proto::ExecuteRequest execute_request =
      CreateExecuteRequest(feature, request_metadata);
  std::string serialized_request;
  execute_request.SerializeToString(&serialized_request);

  HandleTokenRequestFlow(
      IsAccessTokenRequiredForFeature(feature), identity_manager,
      signin::OAuthConsumerId::kOptimizationGuideModelExecution,
      base::BindOnce(&ModelExecutionFetcherImpl::OnAccessTokenReceived,
                     weak_ptr_factory_.GetWeakPtr(), feature,
                     serialized_request, timeout));
}

void ModelExecutionFetcherImpl::OnAccessTokenReceived(
    ModelBasedCapabilityKey feature,
    const std::string& serialized_request,
    std::optional<base::TimeDelta> timeout,
    const std::string& access_token) {
  if (IsAccessTokenRequiredForFeature(feature) && access_token.empty()) {
    RecordRequestStatusHistogram(*model_execution_feature_,
                                 FetcherRequestStatus::kUserNotSignedIn);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kPermissionDenied)));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  // Use API key if no access token is attached.
  resource_request->url = optimization_guide_service_url_;
  if (access_token.empty()) {
    resource_request->url = net::AppendOrReplaceQueryParameter(
        resource_request->url, "key",
        features::GetOptimizationGuideServiceAPIKey());
  } else {
    PopulateAuthorizationRequestHeader(resource_request.get(), access_token);
  }
  if (timeout && timeout->is_positive()) {
    PopulateServerTimeoutRequestHeader(resource_request.get(), *timeout);
  }

  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  AppendHeadersIfNeeded(*resource_request);

  active_url_loader_ = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request),
      // This is always InIncognito::kNo as the server model execution is not
      // enabled on incognito sessions and is rechecked before each fetch.
      variations::InIncognito::kNo, variations::SignedIn::kNo,
      GetNetworkTrafficAnnotation(*model_execution_feature_));

  active_url_loader_->AttachStringForUpload(serialized_request,
                                            "application/x-protobuf");
  active_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ModelExecutionFetcherImpl::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModelExecutionFetcherImpl::OnURLLoadComplete(
    std::optional<std::string> response_body) {
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

  if (response_code >= 0) {
    base::UmaHistogramSparse("OptimizationGuide.ModelExecutionFetcher.Status",
                             response_code);
  }
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.ModelExecutionFetcher.NetErrorCode", -net_error);

  proto::ExecuteResponse execute_response;

  if (net_error != net::OK || response_code != net::HTTP_OK) {
    RecordRequestStatusHistogram(*model_execution_feature_,
                                 FetcherRequestStatus::kResponseError);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromHttpStatusCode(
                static_cast<net::HttpStatusCode>(response_code))));
    return;
  }
  if (!response_body || !execute_response.ParseFromString(*response_body)) {
    RecordRequestStatusHistogram(*model_execution_feature_,
                                 FetcherRequestStatus::kResponseError);
    std::move(model_execution_callback_)
        .Run(base::unexpected(
            OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure)));
    return;
  }

  RecordRequestStatusHistogram(*model_execution_feature_,
                               FetcherRequestStatus::kSuccess);
  // This should be the last call, since the callback could be deleting `this`.
  std::move(model_execution_callback_).Run(base::ok(execute_response));
}

}  // namespace optimization_guide
