// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/access_token_helper.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_service.h"
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

void RecordUploadStatusHistogram(UserVisibleFeatureKey feature,
                                 ModelQualityLogsUploadStatus status) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.",
           GetStringNameForModelExecutionFeature(feature)}),
      status);
}

void RecordUserFeedbackHistogram(UserVisibleFeatureKey feature,
                                 proto::UserFeedback user_feedback) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelQuality.UserFeedback.",
                    GetStringNameForModelExecutionFeature(feature)}),
      static_cast<ModelQualityUserFeedback>(user_feedback));
}

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

// Sets user feedback for the ModelExecutionFeature corresponding to the
// `log_entry`.
void RecordUserFeedbackHistogram(proto::LogAiDataRequest* log_ai_data_request) {
  proto::UserFeedback user_feedback =
      proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
  auto feature =
      GetModelExecutionFeature(log_ai_data_request->feature_case()).value();
  switch (feature) {
    case UserVisibleFeatureKey::kCompose:
      user_feedback =
          GetModelQualityData<ComposeFeatureTypeMap>(log_ai_data_request)
              ->user_feedback();
      RecordUserFeedbackHistogram(feature, user_feedback);
      break;
    case UserVisibleFeatureKey::kTabOrganization:
      // If there is no tab organization, we don't have any user_feedback mark
      // it as unspecified.
      if (GetModelQualityData<TabOrganizationFeatureTypeMap>(
              log_ai_data_request)
              ->organizations()
              .empty()) {
        RecordUserFeedbackHistogram(feature, user_feedback);
      } else if (GetModelQualityData<TabOrganizationFeatureTypeMap>(
                     log_ai_data_request)
                     ->user_feedback()) {
        user_feedback = GetModelQualityData<TabOrganizationFeatureTypeMap>(
                            log_ai_data_request)
                            ->user_feedback();
        RecordUserFeedbackHistogram(feature, user_feedback);
      } else {
        for (auto organization :
             GetModelQualityData<TabOrganizationFeatureTypeMap>(
                 log_ai_data_request)
                 ->organizations()) {
          // TODO(b/331852814): Remove this else case along with the multi tab
          // organization flag
          // We assume there is only one tab organization when we upload the
          // model quality data for this version.
          user_feedback = organization.user_feedback();
          RecordUserFeedbackHistogram(feature, user_feedback);
        }
      }
      break;
    case UserVisibleFeatureKey::kWallpaperSearch:
      user_feedback = GetModelQualityData<WallpaperSearchFeatureTypeMap>(
                          log_ai_data_request)
                          ->user_feedback();
      RecordUserFeedbackHistogram(feature, user_feedback);
      break;
  }
}

// URL load completion callback.
void OnURLLoadComplete(
    std::unique_ptr<network::SimpleURLLoader> active_url_loader,
    UserVisibleFeatureKey feature,
    std::unique_ptr<std::string> response_body) {
  CHECK(active_url_loader) << "loader shouldn't be null\n";
  TRACE_EVENT1("browser", "ModelQualityLogsUploaderService::OnURLLoadComplete",
               "feature", GetStringNameForModelExecutionFeature(feature));

  auto net_error = active_url_loader->NetError();
  int response_code = -1;
  if (active_url_loader->ResponseInfo() &&
      active_url_loader->ResponseInfo()->headers) {
    response_code = active_url_loader->ResponseInfo()->headers->response_code();

    // Only record response code when there are headers.
    base::UmaHistogramSparse(
        "OptimizationGuide.ModelQualityLogsUploaderService.Status",
        response_code);
  }

  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.ModelQualityLogsUploaderService.NetErrorCode",
      -net_error);

  if (net_error != net::OK || response_code != net::HTTP_OK) {
    RecordUploadStatusHistogram(feature,
                                ModelQualityLogsUploadStatus::kNetError);
    return;
  }
  RecordUploadStatusHistogram(feature,
                              ModelQualityLogsUploadStatus::kUploadSuccessful);
}

proto::PerformanceClass GetPerformanceClass(PrefService* local_state) {
  int value = local_state->GetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass);
  OnDeviceModelPerformanceClass performance_class =
      static_cast<OnDeviceModelPerformanceClass>(value);
  switch (performance_class) {
    case OnDeviceModelPerformanceClass::kVeryLow:
      return proto::PERFORMANCE_CLASS_VERY_LOW;
    case OnDeviceModelPerformanceClass::kLow:
      return proto::PERFORMANCE_CLASS_LOW;
    case OnDeviceModelPerformanceClass::kMedium:
      return proto::PERFORMANCE_CLASS_MEDIUM;
    case OnDeviceModelPerformanceClass::kHigh:
      return proto::PERFORMANCE_CLASS_HIGH;
    case OnDeviceModelPerformanceClass::kVeryHigh:
      return proto::PERFORMANCE_CLASS_VERY_HIGH;
    case OnDeviceModelPerformanceClass::kUnknown:
    case OnDeviceModelPerformanceClass::kError:
    case OnDeviceModelPerformanceClass::kServiceCrash:
    case OnDeviceModelPerformanceClass::kGpuBlocked:
    case OnDeviceModelPerformanceClass::kFailedToLoadLibrary:
      return proto::PERFORMANCE_CLASS_UNSPECIFIED;
  }
  return proto::PERFORMANCE_CLASS_UNSPECIFIED;
}

}  // namespace

ModelQualityLogsUploaderService::ModelQualityLogsUploaderService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service)
    : model_quality_logs_uploader_service_url_(
          net::AppendOrReplaceQueryParameter(
              GetModelQualityLogsUploaderServiceURL(),
              "key",
              switches::GetModelQualityServiceAPIKey())),
      pref_service_(pref_service),
      url_loader_factory_(url_loader_factory) {
  CHECK(model_quality_logs_uploader_service_url_.SchemeIs(url::kHttpsScheme));
}

ModelQualityLogsUploaderService::~ModelQualityLogsUploaderService() = default;

bool ModelQualityLogsUploaderService::CanUploadLogs(
    UserVisibleFeatureKey feature) {
  return false;
}

void ModelQualityLogsUploaderService::SetSystemProfileProto(
    proto::LoggingMetadata* logging_metadata) {}

void ModelQualityLogsUploaderService::SetUrlLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void ModelQualityLogsUploaderService::UploadModelQualityLogs(
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't do anything if the data is null. Nothing to upload.
  if (!log_ai_data_request) {
    return;
  }

  // Log User Feedback Histogram corresponding to the LogAiDataRequest.
  RecordUserFeedbackHistogram(log_ai_data_request.get());

  auto feature =
      GetModelExecutionFeature(log_ai_data_request->feature_case()).value();

  TRACE_EVENT1("browser",
               "ModelQualityLogsUploaderService::UploadModelQualityLogs",
               "feature", GetStringNameForModelExecutionFeature(feature));

  // Set the client id for logging if non-zero.
  proto::LoggingMetadata* logging_metadata =
      log_ai_data_request->mutable_logging_metadata();
  int64_t client_id = GetOrCreateModelQualityClientId(feature, pref_service_);
  if (client_id != 0) {
    logging_metadata->set_client_id(client_id);
  }

  SetSystemProfileProto(logging_metadata);

  proto::PerformanceClass perf_class = GetPerformanceClass(pref_service_);
  if (perf_class != proto::PERFORMANCE_CLASS_UNSPECIFIED) {
    logging_metadata->mutable_on_device_system_profile()->set_performance_class(
        perf_class);
  }

  std::string serialized_logs;
  log_ai_data_request->SerializeToString(&serialized_logs);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = model_quality_logs_uploader_service_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("model_quality_logging",
                                          R"(
        semantics {
          sender: "Optimization Guide Model Quality Logger"
          description:
            "Sends logging data about machine learning model requests, "
            "responses and user interaction with the feature using the "
            "machine learning model. Google may use this data to improve "
            "Google products, including the machine learning models "
            "themselves, or to develop new models."
          trigger:
            "Sent after the feature using the machine learning model "
            "is no longer using the model response, and the user is "
            "done interacting with the feature."
          data: "The machine learning model request, response and "
            "usage statistics, feedback data and device information "
            "(platform, chrome version, etc.)."
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
            type: WEB_CONTENT
            type: USER_CONTENT
            type: HW_OS_INFO
          }
          last_reviewed: "2024-01-12"
          destination: GOOGLE_OWNED_SERVICE
        }
         policy {
          cookies_allowed: NO
          setting:
            "Users can disable this by signing out of Chrome or by "
            "disabling each feature using a machine learning model."
          chrome_policy {
            CreateThemesSettings {
              CreateThemesSettings: 1
            }
            TabOrganizerSettings {
              TabOrganizerSettings: 1
            }
            HelpMeWriteSettings {
              HelpMeWriteSettings: 1
            }
          }
          chrome_policy {
            CreateThemesSettings {
              CreateThemesSettings: 2
            }
            TabOrganizerSettings {
              TabOrganizerSettings: 2
            }
            HelpMeWriteSettings {
              HelpMeWriteSettings: 2
            }
          }
        })");

  // Holds the currently active url request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader;
  active_url_loader = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request),
      // This is always InIncognito::kNo as model quality logs upload is not
      // enabled on incognito sessions and is rechecked before each upload.
      variations::InIncognito::kNo, variations::SignedIn::kNo,
      traffic_annotation);

  active_url_loader->AttachStringForUpload(serialized_logs,
                                           "application/x-protobuf");

  auto* active_url_loader_ptr = active_url_loader.get();
  active_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&OnURLLoadComplete, std::move(active_url_loader),
                     feature));
}

}  // namespace optimization_guide
