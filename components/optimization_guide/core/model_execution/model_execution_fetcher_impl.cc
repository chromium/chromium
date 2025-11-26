// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_fetcher_impl.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/access_token_helper.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
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

constexpr char kGoogleAPITypeName[] = "type.googleapis.com/";

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kWallpaperSearch:
      return net::DefineNetworkTrafficAnnotation(
          "wallpaper_create_themes_model_execution",
          R"(
        semantics {
          sender: "Create themes with AI"
          description: "Create a wallpaper with AI for custom themes."
          trigger: "User opens a new tab and clicks Customize Chrome."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "User selected characteristics of the theme such as subject, mood,"
            " visual style and color."
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
          }
          last_reviewed: "2024-01-11"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this by signing-in to Chrome, and from Settings."
          chrome_policy {
            CreateThemesSettings {
              CreateThemesSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kTabOrganization:
      return net::DefineNetworkTrafficAnnotation(
          "tab_organizer_model_execution", R"(
        semantics {
          sender: "Tab organizer"
          description:
            "Automatically creates tab groups based on the open tabs."
          trigger:
            "User right-clicks on a tab and clicks Organize Similar Tabs."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "URL and title of the tabs to organize."
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: SENSITIVE_URL
            type: WEB_CONTENT
          }
          last_reviewed: "2024-01-11"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this by signing-in to Chrome, and from Settings."
          chrome_policy {
            TabOrganizerSettings {
              TabOrganizerSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kCompose:
      return net::DefineNetworkTrafficAnnotation(
          "help_me_write_model_execution", R"(
        semantics {
          sender: "Help me write"
          description:
            "Helps users to write content in a web form, such as for product "
            "reviews or emails."
          trigger: "User right-clicks on a text box and clicks Help me write."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "User written input text, title, URL, and content of the page"
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: SENSITIVE_URL
            type: WEB_CONTENT
            type: USER_CONTENT
          }
          last_reviewed: "2024-01-11"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this by signing-in to Chrome, and from Settings."
          chrome_policy {
            HelpMeWriteSettings {
              HelpMeWriteSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return net::DefineNetworkTrafficAnnotation(
          "password_change_submission_model_execution", R"(
        semantics {
          sender: "Automated Password Change"
          description:
            "Analyze page content to find elements that open and submit"
            " password change forms for Chrome actuation."
            " Lastly identities if the password change was successful."
          trigger:
            "User logged-in with a compromised credential and accepted "
            "an option from the dialog to change password automatically."
          destination: GOOGLE_OWNED_SERVICE
          data:
            "Title, URL, and content of the page, which may "
            "potentially contain user input."
          internal {
            contacts {
              email: "chrome-intelligence-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: SENSITIVE_URL
            type: WEB_CONTENT
          }
          last_reviewed: "2026-07-03"
        }
        policy {
          cookies_allowed: NO
          setting:
            "There is no dedicated setting for this feature."
            "Users are free to choose whether to use the feature "
            "or not when it's offered."
          chrome_policy {
            AutomatedPasswordChangeSettings {
              AutomatedPasswordChangeSettings: 2
            }
          }
        })");
    case ModelBasedCapabilityKey::kTest:
    case ModelBasedCapabilityKey::kBlingPrototyping:
      // Used for testing purposes. No real features use this.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kFormsClassifications:
      return net::DefineNetworkTrafficAnnotation(
          "forms_classifications_model_execution", R"(
    semantics {
      sender: "AutofillAI - Forms Classifications"
      description:
        "Analyze page content to classify the types of form fields and store "
        "those classifications for subsequent autofilling of forms."
      trigger: "User encounters a web form on page load and the Autofill "
               "server has selected the form as relevant for model execution."
      destination: GOOGLE_OWNED_SERVICE
      data: "Title, URL, and content of the page."
      internal {
        contacts {
          email: "chrome-intelligence-core@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
        type: SENSITIVE_URL
        type: WEB_CONTENT
        type: USER_CONTENT
      }
      last_reviewed: "2025-04-23"
    }
    policy {
      cookies_allowed: NO
      setting:
        "Users can control this by signing-in to Chrome, and via the "
        "'Autofill with AI' setting in the 'Autofill and passwords' "
        "section."
      chrome_policy {
        AutofillPredictionSettings {
          AutofillPredictionSettings: 2
        }
      }
    })");
    case ModelBasedCapabilityKey::kEnhancedCalendar:
      // TODO(crbug.com/398296762): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return net::DefineNetworkTrafficAnnotation(
          "zero_state_suggestions_model_execution", R"(
    semantics {
      sender: "Gemini in Chrome - Zero State Suggestions"
      description:
        "Generates contextual suggestions about the current page when Gemini "
        "in Chrome does not have a query."
      trigger:
        "User opens Gemini in Chrome via browser entrypoint, OS entrypoint, or"
        " hot key."
      destination: GOOGLE_OWNED_SERVICE
      data:
        "Title, URL, and content of the page, which may potentially contain "
        "user input. The access token is also sent to verify user is of "
        "sufficient age to use Gemini in Chrome."
      internal {
        contacts {
          email: "chrome-intelligence-core@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
        type: SENSITIVE_URL
        type: WEB_CONTENT
      }
      last_reviewed: "2025-05-21"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature can be disabled via GeminiSettings."
      chrome_policy {
        GeminiSettings {
          GeminiSettings: 1
        }
      }
    })");
    case ModelBasedCapabilityKey::kWalletablePassExtraction:
      // TODO(crbug.com/441680019): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kAmountExtraction:
      return net::DefineNetworkTrafficAnnotation(
          "amount_extraction_model_execution",
          R"(
    semantics {
      sender: "Amount Extraction"
      description:
        "Uses server-side AI model to extract the final checkout amount "
        "from a web page to support features like Buy Now Pay Later (BNPL). "
        "This helps improve the accuracy of amount extraction on checkout "
        "pages."
      trigger:
        "User navigates to a checkout page on a supported merchant website "
        "and selects bnpl option."
      destination: GOOGLE_OWNED_SERVICE
      data:
        "The text content of the checkout page, which may include the page "
        "URL, and user-input data in the checkout form."
      internal {
        contacts {
          email: "chrome-intelligence-core@google.com"
        }
      }
      user_data {
        type: WEB_CONTENT
        type: USER_CONTENT
      }
      last_reviewed: "2025-11-20"
    }
    policy {
      cookies_allowed: NO
      setting:
        "You can enable or disable this feature via the 'Pay over time' toggle "
        "in Chrome Settings > Autofill and Passwords > Payment methods. This "
        "feature is enabled by default. It also requires "
        "'Save and fill payment methods' to be enabled."
      chrome_policy {
        AutofillCreditCardEnabled {
          AutofillCreditCardEnabled: false
        }
      }
    })");
    case ModelBasedCapabilityKey::kIosSmartTabGrouping:
      // TODO(crbug.com/456457419): Add network traffic annotation.
      return MISSING_TRAFFIC_ANNOTATION;
    case ModelBasedCapabilityKey::kHistorySearch:
      // On-device only feature.
      NOTREACHED();
  }
}

void RecordRequestStatusHistogram(ModelBasedCapabilityKey feature,
                                  FetcherRequestStatus status) {
  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ModelExecutionFetcher.RequestStatus.",
                    GetStringNameForModelExecutionFeature(feature)}),
      status);
}

// Appends headers as specified by the command line arguments.
void AppendHeadersIfNeeded(network::ResourceRequest& request) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kModelExecutionEnableRemoteDebugLogging)) {
    return;
  }
  request.headers.SetHeaderIfMissing(
      kOptimizationGuideModelExecutionDebugLogsHeaderKey, "");
}

// Returns whether model executions for the `feature` require an access token.
bool IsAccessTokenRequiredForFeature(ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kCompose:
    case ModelBasedCapabilityKey::kTabOrganization:
    case ModelBasedCapabilityKey::kWallpaperSearch:
    case ModelBasedCapabilityKey::kTest:
    case ModelBasedCapabilityKey::kHistorySearch:
    case ModelBasedCapabilityKey::kBlingPrototyping:
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
    case ModelBasedCapabilityKey::kEnhancedCalendar:
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
    case ModelBasedCapabilityKey::kWalletablePassExtraction:
    case ModelBasedCapabilityKey::kAmountExtraction:
    case ModelBasedCapabilityKey::kIosSmartTabGrouping:
      return true;
    case ModelBasedCapabilityKey::kFormsClassifications:
      return !base::FeatureList::IsEnabled(
          features::kOptimizationGuideBypassFormsClassificationAuth);
  }
}

}  // namespace

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

  proto::ExecuteRequest execute_request;
  execute_request.set_feature(ToModelExecutionFeatureProto(feature));
  proto::Any* any_metadata = execute_request.mutable_request_metadata();
  any_metadata->set_type_url(
      base::StrCat({kGoogleAPITypeName, request_metadata.GetTypeName()}));
  request_metadata.SerializeToString(any_metadata->mutable_value());
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
  if (!access_token.empty()) {
    PopulateAuthorizationRequestHeader(resource_request.get(), access_token);
  }
  if (timeout && timeout->is_positive()) {
    PopulateServerTimeoutRequestHeader(resource_request.get(), *timeout);
  }

  resource_request->url = optimization_guide_service_url_;
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
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecutionFetcher.FetchLatency.",
           GetStringNameForModelExecutionFeature(*model_execution_feature_)}),
      base::TimeTicks::Now() - fetch_start_time_);
  RecordRequestStatusHistogram(*model_execution_feature_,
                               FetcherRequestStatus::kSuccess);
  // This should be the last call, since the callback could be deleting `this`.
  std::move(model_execution_callback_).Run(base::ok(execute_response));
}

}  // namespace optimization_guide
