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
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_conversion_util.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/switches.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
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

std::string GetAPIKeyForUrl(version_info::Channel channel) {
  return google_apis::GetAPIKey(channel);
}

template <typename ProtoType, typename ResultType, typename ReturnType>
base::OnceCallback<void(std::optional<std::string>)> BindParseAndConvert(
    base::OnceCallback<void(std::optional<ResultType>)> callback,
    ReturnType (*convert_func)(const ProtoType&)) {
  return base::BindOnce(
      [](base::OnceCallback<void(std::optional<ResultType>)> cb,
         ReturnType (*conv)(const ProtoType&),
         std::optional<std::string> response_body) {
        if (ProtoType proto; response_body && proto.ParseFromString(*response_body)) {
          std::move(cb).Run(conv(proto));
        } else {
          std::move(cb).Run(std::nullopt);
        }
      },
      std::move(callback), convert_func);
}

}  // namespace

// static
std::unique_ptr<AnnotationIndexClient> AnnotationIndexClient::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel) {
  return std::make_unique<AnnotationIndexClientImpl>(
      std::move(url_loader_factory), channel);
}

// TODO(crbug.com/483673955): Add UMA metrics for latency, traffic, and error
// tracking.
AnnotationIndexClientImpl::AnnotationIndexClientImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel)
    : AnnotationIndexClientImpl(std::move(url_loader_factory),
                                GetAPIKeyForUrl(channel)) {}

AnnotationIndexClientImpl::AnnotationIndexClientImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string api_key)
    : url_loader_factory_(std::move(url_loader_factory)),
      api_key_(std::move(api_key)) {}

AnnotationIndexClientImpl::~AnnotationIndexClientImpl() = default;

void AnnotationIndexClientImpl::GetFilterSuggestionCandidates(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations,
    base::OnceCallback<
        void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback) {
  GURL api_base_url = GetIndexServerApiBaseUrl();
  if (!api_base_url.is_valid()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ExecuteRequest(
      CreatePostResourceRequest(api_base_url,
                                kGetTaskExecutionStrategiesEndpoint),
      ToGetTaskExecutionStrategiesRequest(url, filter_annotations)
          .SerializeAsString(),
      BindParseAndConvert(std::move(callback), &ToFilterSuggestionCandidates));
}

void AnnotationIndexClientImpl::GetSupportedTaskTypesForDomain(
    std::string_view domain,
    base::OnceCallback<void(std::optional<std::vector<std::string>>)>
        callback) {
  GURL api_base_url = GetIndexServerApiBaseUrl();
  if (!api_base_url.is_valid()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ExecuteRequest(
      CreatePostResourceRequest(api_base_url, kGetSupportedTasksEndpoint),
      ToGetSupportedTasksRequest(domain).SerializeAsString(),
      BindParseAndConvert(std::move(callback), &ToSupportedTasks));
}

void AnnotationIndexClientImpl::ExtractFilterAnnotation(
    const GURL& url,
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback) {
  GURL api_base_url = GetIndexServerApiBaseUrl();
  if (!api_base_url.is_valid()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ExecuteRequest(
      CreatePostResourceRequest(api_base_url, kExtractTaskAttributesEndpoint),
      ToExtractTaskAttributesRequest(url).SerializeAsString(),
      BindParseAndConvert(std::move(callback), &ToFilterAnnotation));
}

std::unique_ptr<network::ResourceRequest>
AnnotationIndexClientImpl::CreatePostResourceRequest(
    const GURL& api_base_url,
    std::string_view endpoint) const {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = api_base_url.Resolve(endpoint);
  request->method = kPostMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Add API key to the request if a key exists, and the endpoint is trusted by
  // Google.
  if (!api_key_.empty() && request->url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(request->url)) {
    google_apis::AddAPIKeyToRequest(*request, api_key_);
  }

  return request;
}

void AnnotationIndexClientImpl::ExecuteRequest(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    base::OnceCallback<void(std::optional<std::string>)> callback) {
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
                     std::move(callback)),
      kMaxDownloadSize);
}

void AnnotationIndexClientImpl::OnSimpleURLLoaderComplete(
    SimpleURLLoaderList::iterator loader_it,
    base::OnceCallback<void(std::optional<std::string>)> callback,
    std::optional<std::string> response_body) {
  network::SimpleURLLoader* loader = loader_it->get();
  int response_code = -1;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }
  const bool is_success = IsHttpSuccess(response_code) && response_body;

  active_url_loaders_.erase(loader_it);

  if (!is_success) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(std::move(response_body));
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
