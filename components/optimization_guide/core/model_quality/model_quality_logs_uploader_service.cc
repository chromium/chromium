// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/access_token_helper.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace optimization_guide {

namespace {

// Returns the URL endpoint for the model quality service along with the needed
// API key.
GURL GetModelQualityLogsUploaderServiceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kModelQualityServiceURL)) {
    return GURL(
        command_line->GetSwitchValueASCII(switches::kModelQualityServiceURL));
  }
  return GURL(kOptimizationGuideServiceModelQualtiyDefaultURL);
}

proto::ModelExecutionFeature GetModelExecutionFeature(
    proto::LogAiDataRequest::FeatureCase feature) {
  switch (feature) {
    case proto::LogAiDataRequest::FeatureCase::kCompose:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE;
    case proto::LogAiDataRequest::FeatureCase::kTabOrganization:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION;
    case proto::LogAiDataRequest::FeatureCase::kWallpaperSearch:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH;
    case proto::LogAiDataRequest::FeatureCase::kDefault:
      NOTREACHED();
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED;
    case proto::LogAiDataRequest::FeatureCase::FEATURE_NOT_SET:
      NOTREACHED();
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED;
  }
}

// URL load completion callback.
void OnURLLoadComplete(
    std::unique_ptr<network::SimpleURLLoader> active_url_loader,
    std::unique_ptr<std::string> response_body) {
  CHECK(active_url_loader) << "loader shouldn't be null\n";
  auto net_error = active_url_loader->NetError();
  int response_code = -1;
  if (active_url_loader->ResponseInfo() &&
      active_url_loader->ResponseInfo()->headers) {
    response_code = active_url_loader->ResponseInfo()->headers->response_code();
  }

  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelQualityLogsUploaderService.Status",
      static_cast<net::HttpStatusCode>(response_code),
      net::HTTP_VERSION_NOT_SUPPORTED);
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.ModelQualityLogsUploaderService.NetErrorCode",
      -net_error);
}

}  // namespace

ModelQualityLogsUploaderService::ModelQualityLogsUploaderService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : model_quality_logs_uploader_service_url_(
          net::AppendOrReplaceQueryParameter(
              GetModelQualityLogsUploaderServiceURL(),
              "key",
              switches::GetModelQualityServiceAPIKey())),
      url_loader_factory_(url_loader_factory) {
  CHECK(model_quality_logs_uploader_service_url_.SchemeIs(url::kHttpsScheme));
}

ModelQualityLogsUploaderService::~ModelQualityLogsUploaderService() = default;

void ModelQualityLogsUploaderService::UploadModelQualityLogs(
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  UploadModelQualityLogs(std::move(log_entry.get()->log_ai_data_request_));
}

void ModelQualityLogsUploaderService::UploadModelQualityLogs(
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't do anything if the data is null. Nothing to upload.
  if (!log_ai_data_request) {
    return;
  }

  // Don't do anything if logging is disabled for the feature. Nothing to
  // upload.
  if (!features::IsModelQualityLoggingEnabledForFeature(
          GetModelExecutionFeature(log_ai_data_request->feature_case()))) {
    return;
  }

  // TODO(b/301301447): Set LoggingMetadata fields during upload.
  proto::LoggingMetadata logging_metadata;
  *(log_ai_data_request->mutable_logging_metadata()) = logging_metadata;

  std::string serialized_logs;
  log_ai_data_request->SerializeToString(&serialized_logs);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = model_quality_logs_uploader_service_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Holds the currently active url request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader;
  active_url_loader = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request),
      // This is always InIncognito::kNo as model quality logs upload is not
      // enabled on incognito sessions and is rechecked before each upload.
      variations::InIncognito::kNo, variations::SignedIn::kNo,
      // TODO(crbug/1485313): Update the traffic annotations with more details
      // about the features.
      MISSING_TRAFFIC_ANNOTATION);

  active_url_loader->AttachStringForUpload(serialized_logs,
                                           "application/x-protobuf");

  auto* active_url_loader_ptr = active_url_loader.get();
  active_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&OnURLLoadComplete, std::move(active_url_loader)));
}

}  // namespace optimization_guide
