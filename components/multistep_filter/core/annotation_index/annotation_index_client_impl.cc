// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/google/core/common/google_util.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_conversion_util.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/switches.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/channel.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

// The MIME type used when uploading Protocol Buffer data.
constexpr std::string_view kApplicationProtobufContentType =
    "application/x-protobuf";

// The maximum allowed download size for API responses (1 MB).
constexpr size_t kMaxDownloadSize = 1024 * 1024;

// The timeout duration for network requests.
constexpr base::TimeDelta kNetworkRequestTimeout = base::Seconds(10);

// This allows reading the error message within the API response when status
// is not 200 (e.g., 400). Otherwise, URL loader will not give any content in
// the response when there is a failure, which makes debugging hard.
constexpr bool kAllowHttpErrorResults = true;

constexpr char kPostMethod[] = "POST";

// `SiteAutomationIndexServer` API endpoints.
constexpr std::string_view kGetTaskExecutionStrategiesEndpoint =
    "GetTaskExecutionStrategies";
constexpr std::string_view kGetSupportedTasksEndpoint = "GetSupportedTasks";
constexpr std::string_view kExtractTaskAttributesEndpoint =
    "ExtractTaskAttributes";

// Network traffic annotation for `SiteAutomationIndexServer` API calls.
constexpr net::NetworkTrafficAnnotationTag
    kMultiStepFilterServerRequestsTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "multistep_filter_server_requests",
            R"(
          semantics {
            sender: "Multistep Filter Service"
            description:
              "The Multistep Filter feature helps users automatically re-apply "
              "their historical filtering preferences (e.g., price ranges or "
              "categories) on supported websites. To enable this, Chrome "
              "communicates with a Google server to evaluate if a website "
              "supports filtering automation, extract the user's active "
              "filtering preferences from the current URL to save them "
              "locally, and retrieve instructions (such as a constructed URL) "
              "for re-applying the user's previously saved filters."
            trigger:
              "When a signed-in user navigates to a supported webpage."
            data:
              "The request may include the domain of the current page, the "
              "full page URL, and a list of the user's previously applied "
              "filter key-value pairs."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "magic-journeys-eng@google.com"
              }
            }
            user_data {
              type: SENSITIVE_URL
              type: USER_CONTENT
            }
            last_reviewed: "2026-03-20"
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is currently in development and is not yet exposed "
              "publicly to users via settings."
            policy_exception_justification:
              "User-facing controls via chrome://flags are under development."
              "Tracking bug: b/485180737."
              "Enterprise policy for this feature is also under development."
              "Tracking bug: b/494568600."
              "The core feature is tracked in b/483675770."
          })");

// Determines whether a HTTP request was successful based on its response code.
bool IsHttpSuccess(int response_code) {
  return response_code >= 200 && response_code < 300;
}

void LogServerRequestFailed(MultistepFilterLogRouter* log_router,
                            int64_t navigation_id,
                            std::string_view domain,
                            std::string_view failure_reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestFailed, domain)
      << LogDetail("failure_reason", std::string(failure_reason));
}

void LogGetTaskExecutionStrategiesRequestSent(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view domain,
    std::string_view request_url,
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations) {
  std::vector<std::string> annotation_strings;
  annotation_strings.reserve(filter_annotations.size());
  for (const FilterAnnotation& annotation : filter_annotations) {
    annotation_strings.push_back(annotation.ToString());
  }
  std::string filter_annotations_str =
      base::StrCat({"[", base::JoinString(annotation_strings, ", "), "]"});

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, domain)
      << LogDetail("request_url", std::string(request_url))
      << LogDetail("current_url", url.spec())
      << LogDetail("execution_candidate_count",
                   static_cast<int>(filter_annotations.size()))
      << LogDetail("execution_candidates", filter_annotations_str);
}

void LogGetSupportedTasksRequestSent(MultistepFilterLogRouter* log_router,
                                     int64_t navigation_id,
                                     std::string_view domain,
                                     std::string_view request_url) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, domain)
      << LogDetail("request_url", std::string(request_url));
}

void LogExtractTaskAttributesRequestSent(MultistepFilterLogRouter* log_router,
                                         int64_t navigation_id,
                                         std::string_view domain,
                                         std::string_view request_url,
                                         const GURL& url) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerRequestSent, domain)
      << LogDetail("request_url", std::string(request_url))
      << LogDetail("source_raw_url", url.spec());
}

void LogServerResponseReceived(MultistepFilterLogRouter* log_router,
                               int64_t navigation_id,
                               std::string_view domain,
                               int response_code,
                               bool is_success) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, domain)
      << LogDetail("is_success", is_success)
      << LogDetail("response_code", response_code);
}

void LogResponseObjectsReceived(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view domain,
    int response_code,
    bool is_success,
    const std::optional<std::vector<FilterSuggestionCandidate>>& result) {
  std::string candidates_str;
  if (result.has_value()) {
    std::vector<std::string> candidate_strings;
    candidate_strings.reserve(result->size());
    for (const FilterSuggestionCandidate& candidate : *result) {
      candidate_strings.push_back(candidate.ToString());
    }
    candidates_str =
        base::StrCat({"[", base::JoinString(candidate_strings, ", "), "]"});
  }

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, domain)
      << LogDetail("is_success", is_success)
      << LogDetail("response_code", response_code)
      << LogDetail("filter_suggestion_candidates_count",
                   result.has_value() ? static_cast<int>(result->size()) : 0)
      << LogDetail("filter_suggestion_candidates", candidates_str);
}

void LogResponseObjectsReceived(
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view domain,
    int response_code,
    bool is_success,
    const std::optional<std::vector<std::string>>& result) {
  std::string tasks_str;
  if (result.has_value()) {
    tasks_str = base::StrCat({"[", base::JoinString(*result, ", "), "]"});
  }

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, domain)
      << LogDetail("is_success", is_success)
      << LogDetail("response_code", response_code)
      << LogDetail("supported_tasks_count",
                   result.has_value() ? static_cast<int>(result->size()) : 0)
      << LogDetail("supported_tasks", tasks_str);
}

void LogResponseObjectsReceived(MultistepFilterLogRouter* log_router,
                                int64_t navigation_id,
                                std::string_view domain,
                                int response_code,
                                bool is_success,
                                const std::optional<FilterAnnotation>& result) {
  std::string annotation_str = result.has_value() ? result->ToString() : "";

  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseReceived, domain)
      << LogDetail("is_success", is_success)
      << LogDetail("response_code", response_code)
      << LogDetail("extracted_attributes_count",
                   result.has_value()
                       ? static_cast<int>(result->attributes.size())
                       : 0)
      << LogDetail("extracted_annotation", annotation_str);
}

void LogServerResponseMalformed(MultistepFilterLogRouter* log_router,
                                int64_t navigation_id,
                                std::string_view domain,
                                std::string_view failure_reason) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kServerResponseMalformed, domain)
      << LogDetail("failure_reason", std::string(failure_reason));
}

template <typename ProtoType, typename ResultType, typename ReturnType>
base::OnceCallback<void(std::optional<std::string>, int)> BindParseAndConvert(
    base::OnceCallback<void(std::optional<ResultType>)> callback,
    ReturnType (*convert_func)(const ProtoType&),
    MultistepFilterLogRouter* log_router,
    int64_t navigation_id,
    std::string_view domain) {
  return base::BindOnce(
      [](base::OnceCallback<void(std::optional<ResultType>)> callback,
         ReturnType (*conv)(const ProtoType&),
         MultistepFilterLogRouter* log_router, int64_t navigation_id,
         std::string domain, std::optional<std::string> response_body,
         int response_code) {
        std::optional<ResultType> result;
        if (response_body) {
          if (ProtoType proto; proto.ParseFromString(*response_body)) {
            result = conv(proto);
            LogResponseObjectsReceived(log_router, navigation_id, domain,
                                       response_code, /*is_success=*/true,
                                       result);
          } else {
            LogServerResponseMalformed(log_router, navigation_id, domain,
                                       "parsing_failed");
          }
        }
        std::move(callback).Run(std::move(result));
      },
      std::move(callback), convert_func,
      // Using `base::Unretained()` is safe because the
      // `MultistepFilterLogRouter` is owned by the `MultistepFilterService`,
      // which outlives `AnnotationIndexClientImpl`, and all pending loaders
      // are canceled instantly on destruction of the
      // `AnnotationIndexClientImpl`, preventing this callback from running.
      base::Unretained(log_router), navigation_id, std::string(domain));
}

std::unique_ptr<network::ResourceRequest> CreatePostResourceRequest(
    const GURL& api_base_url,
    std::string_view endpoint) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = api_base_url.Resolve(endpoint);
  request->method = kPostMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return request;
}

}  // namespace

// static
std::unique_ptr<AnnotationIndexClient> AnnotationIndexClient::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    MultistepFilterLogRouter* log_router) {
  return std::make_unique<AnnotationIndexClientImpl>(
      std::move(url_loader_factory), identity_manager, log_router);
}

// TODO(crbug.com/483673955): Add UMA metrics for latency, traffic, and error
// tracking.
AnnotationIndexClientImpl::AnnotationIndexClientImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    MultistepFilterLogRouter* log_router)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      log_router_(log_router) {}

AnnotationIndexClientImpl::~AnnotationIndexClientImpl() = default;

void AnnotationIndexClientImpl::GetFilterSuggestionCandidates(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations,
    base::OnceCallback<
        void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback,
    int64_t navigation_id) {
  const std::string domain = GetEtldPlusOne(url);
  GURL api_base_url = GetIndexServerApiBaseUrl();
  if (!api_base_url.is_valid()) {
    LogServerRequestFailed(log_router_, navigation_id, domain,
                           "invalid_api_base_url");
    std::move(callback).Run(std::nullopt);
    return;
  }

  GetTaskExecutionStrategiesRequest proto =
      ToGetTaskExecutionStrategiesRequest(url, filter_annotations);
  LogGetTaskExecutionStrategiesRequestSent(
      log_router_, navigation_id, domain,
      api_base_url.Resolve(kGetTaskExecutionStrategiesEndpoint).spec(), url,
      filter_annotations);

  ExecuteRequest(
      CreatePostResourceRequest(api_base_url,
                                kGetTaskExecutionStrategiesEndpoint),
      proto.SerializeAsString(),
      BindParseAndConvert(std::move(callback), &ToFilterSuggestionCandidates,
                          log_router_, navigation_id, domain),
      navigation_id, domain);
}

void AnnotationIndexClientImpl::GetSupportedTaskTypesForDomain(
    std::string_view domain,
    base::OnceCallback<void(std::optional<std::vector<std::string>>)> callback,
    int64_t navigation_id) {
  GURL api_base_url = GetIndexServerApiBaseUrl();
  if (!api_base_url.is_valid()) {
    LogServerRequestFailed(log_router_, navigation_id, domain,
                           "invalid_api_base_url");
    std::move(callback).Run(std::nullopt);
    return;
  }

  GetSupportedTasksRequest proto = ToGetSupportedTasksRequest(domain);
  LogGetSupportedTasksRequestSent(
      log_router_, navigation_id, domain,
      api_base_url.Resolve(kGetSupportedTasksEndpoint).spec());

  ExecuteRequest(
      CreatePostResourceRequest(api_base_url, kGetSupportedTasksEndpoint),
      proto.SerializeAsString(),
      BindParseAndConvert(std::move(callback), &ToSupportedTasks, log_router_,
                          navigation_id, std::string(domain)),
      navigation_id, std::string(domain));
}

void AnnotationIndexClientImpl::ExtractFilterAnnotation(
    const GURL& url,
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
    int64_t navigation_id) {
  std::string domain = GetEtldPlusOne(url);
  GURL api_base_url = GetIndexServerApiBaseUrl();
  if (!api_base_url.is_valid()) {
    LogServerRequestFailed(log_router_, navigation_id, domain,
                           "invalid_api_base_url");
    std::move(callback).Run(std::nullopt);
    return;
  }

  ExtractTaskAttributesRequest proto = ToExtractTaskAttributesRequest(url);
  LogExtractTaskAttributesRequestSent(
      log_router_, navigation_id, domain,
      api_base_url.Resolve(kExtractTaskAttributesEndpoint).spec(), url);

  ExecuteRequest(
      CreatePostResourceRequest(api_base_url, kExtractTaskAttributesEndpoint),
      proto.SerializeAsString(),
      BindParseAndConvert(std::move(callback), &ToFilterAnnotation, log_router_,
                          navigation_id, domain),
      navigation_id, domain);
}

void AnnotationIndexClientImpl::ExecuteRequest(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    base::OnceCallback<void(std::optional<std::string>, int)> callback,
    int64_t navigation_id,
    std::string_view domain) {
  if (!request->url.SchemeIs(url::kHttpsScheme) ||
      !google_util::IsGoogleAssociatedDomainUrl(request->url)) {
    StartLoader(std::move(request), std::move(request_body),
                std::move(callback), navigation_id, domain);
    return;
  }

  if (!identity_manager_) {
    LogServerRequestFailed(log_router_, navigation_id, domain,
                           "no_identity_manager");
    std::move(callback).Run(std::nullopt, -1);
    return;
  }

  const CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    LogServerRequestFailed(log_router_, navigation_id, domain,
                           "user_not_signed_in");
    std::move(callback).Run(std::nullopt, -1);
    return;
  }

  const base::UnguessableToken fetcher_id = base::UnguessableToken::Create();
  auto finished = base::MakeRefCounted<base::RefCountedData<bool>>(false);

  std::unique_ptr<signin::AccessTokenFetcher> fetcher =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          account_id, signin::OAuthConsumerId::kMultistepFilter,
          base::BindOnce(
              [](scoped_refptr<base::RefCountedData<bool>> fin,
                 base::WeakPtr<AnnotationIndexClientImpl> client,
                 base::UnguessableToken fetcher_id,
                 std::unique_ptr<network::ResourceRequest> request,
                 std::string request_body,
                 base::OnceCallback<void(std::optional<std::string>, int)>
                     callback,
                 int64_t navigation_id, std::string domain,
                 GoogleServiceAuthError error,
                 signin::AccessTokenInfo access_token_info) {
                fin->data = true;
                if (client) {
                  client->OnAccessTokenFetched(
                      fetcher_id, std::move(request), std::move(request_body),
                      std::move(callback), error, access_token_info,
                      navigation_id, std::move(domain));
                }
              },
              finished, weak_ptr_factory_.GetWeakPtr(), fetcher_id,
              std::move(request), std::move(request_body), std::move(callback),
              navigation_id, std::string(domain)),
          signin::AccessTokenFetcher::Mode::kImmediate);

  if (!finished->data) {
    active_fetchers_[fetcher_id] = std::move(fetcher);
  }
}

void AnnotationIndexClientImpl::OnAccessTokenFetched(
    base::UnguessableToken fetcher_id,
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    base::OnceCallback<void(std::optional<std::string>, int)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info,
    int64_t navigation_id,
    std::string_view domain) {
  active_fetchers_.erase(fetcher_id);

  if (error.state() != GoogleServiceAuthError::NONE) {
    LogServerRequestFailed(log_router_, navigation_id, domain,
                           "oauth_fetch_failed");
    std::move(callback).Run(std::nullopt, -1);
    return;
  }

  DCHECK(request->url.SchemeIs(url::kHttpsScheme));
  DCHECK(google_util::IsGoogleAssociatedDomainUrl(request->url));

  request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                             "Bearer " + access_token_info.token);
  StartLoader(std::move(request), std::move(request_body), std::move(callback),
              navigation_id, domain);
}

void AnnotationIndexClientImpl::StartLoader(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    base::OnceCallback<void(std::optional<std::string>, int)> callback,
    int64_t navigation_id,
    std::string_view domain) {
  active_url_loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(request), kMultiStepFilterServerRequestsTrafficAnnotation));
  auto loader_it = std::prev(active_url_loaders_.end());
  network::SimpleURLLoader* loader = loader_it->get();

  loader->AttachStringForUpload(std::move(request_body),
                                kApplicationProtobufContentType);
  loader->SetAllowHttpErrorResults(kAllowHttpErrorResults);
  loader->SetTimeoutDuration(kNetworkRequestTimeout);
  loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&AnnotationIndexClientImpl::OnSimpleURLLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), loader_it,
                     std::move(callback), navigation_id, std::string(domain)),
      kMaxDownloadSize);
}

void AnnotationIndexClientImpl::OnSimpleURLLoaderComplete(
    SimpleURLLoaderList::iterator loader_it,
    base::OnceCallback<void(std::optional<std::string>, int)> callback,
    int64_t navigation_id,
    std::string_view domain,
    std::optional<std::string> response_body) {
  network::SimpleURLLoader* loader = loader_it->get();
  int response_code = -1;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }
  const bool is_success = IsHttpSuccess(response_code) && response_body;

  active_url_loaders_.erase(loader_it);

  if (!is_success) {
    LogServerResponseReceived(log_router_, navigation_id, domain, response_code,
                              is_success);
    std::move(callback).Run(std::nullopt, response_code);
    return;
  }

  std::move(callback).Run(std::move(response_body), response_code);
}

GURL AnnotationIndexClientImpl::GetIndexServerApiBaseUrl() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kMultistepFilterIndexServerApiBaseUrl)) {
    return GURL(command_line->GetSwitchValueASCII(
        switches::kMultistepFilterIndexServerApiBaseUrl));
  }
  return GURL(kMultistepFilterIndexServerApiBaseUrl.Get());
}

}  // namespace multistep_filter
