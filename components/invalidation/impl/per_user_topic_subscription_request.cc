// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_subscription_request.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using net::HttpRequestHeaders;

namespace invalidation {

namespace {

const char kPublicTopicNameKey[] = "publicTopicName";
const char kPrivateTopicNameKey[] = "privateTopicName";

const std::string* GetTopicName(const base::Value& value) {
  if (!value.is_dict())
    return nullptr;
  const base::Value::Dict& dict = value.GetDict();
  if (dict.FindBool("isPublic").value_or(false)) {
    return dict.FindString(kPublicTopicNameKey);
  }
  return dict.FindString(kPrivateTopicNameKey);
}

bool IsNetworkError(int net_error) {
  // Note: ERR_HTTP_RESPONSE_CODE_FAILURE isn't a real network error - it
  // indicates that the network request itself succeeded, but we received an
  // HTTP error. The alternative to special-casing this error would be to call
  // SetAllowHttpErrorResults(true) on the SimpleUrlLoader, but that also means
  // we'd download the response body in case of HTTP errors, which we don't
  // need.
  return net_error != net::OK &&
         net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE;
}

// Subscription status values for UMA_HISTOGRAM.
enum class SubscriptionStatus {
  kSuccess = 0,
  kNetworkFailure = 1,
  kHttpFailure = 2,
  kParsingFailure = 3,
  kFailure = 4,
  kMaxValue = kFailure,
};

void RecordRequestStatus(SubscriptionStatus status,
                         PerUserTopicSubscriptionRequest::RequestType type,
                         const std::string& topic,
                         int net_error = net::OK,
                         int response_code = 200) {
  switch (type) {
    case PerUserTopicSubscriptionRequest::RequestType::kSubscribe: {
      base::UmaHistogramEnumeration(
          "FCMInvalidations.SubscriptionRequestStatus", status);
      break;
    }
    case PerUserTopicSubscriptionRequest::RequestType::kUnsubscribe: {
      base::UmaHistogramEnumeration(
          "FCMInvalidations.UnsubscriptionRequestStatus", status);
      break;
    }
  }
  if (type != PerUserTopicSubscriptionRequest::RequestType::kSubscribe) {
    return;
  }

  if (IsNetworkError(net_error)) {
    // Tracks the cases, when request fails due to network error.
    base::UmaHistogramSparse("FCMInvalidations.FailedSubscriptionsErrorCode",
                             -net_error);
    DVLOG(1) << "Subscription request failed with error: " << net_error << ": "
             << net::ErrorToString(net_error);
  } else {
    // Log a histogram to track response success vs. failure rates.
    base::UmaHistogramSparse("FCMInvalidations.SubscriptionResponseCode",
                             response_code);
  }
}

}  // namespace

PerUserTopicSubscriptionRequest::PerUserTopicSubscriptionRequest() = default;

PerUserTopicSubscriptionRequest::~PerUserTopicSubscriptionRequest() = default;

void PerUserTopicSubscriptionRequest::Start(
    CompletedCallback callback,
    network::mojom::URLLoaderFactory* loader_factory) {
  DCHECK(request_completed_callback_.is_null()) << "Request already running!";
  request_completed_callback_ = std::move(callback);

  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory,
      base::BindOnce(&PerUserTopicSubscriptionRequest::OnURLFetchComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PerUserTopicSubscriptionRequest::OnURLFetchComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (simple_loader_->ResponseInfo() &&
      simple_loader_->ResponseInfo()->headers) {
    response_code = simple_loader_->ResponseInfo()->headers->response_code();
  }
  OnURLFetchCompleteInternal(simple_loader_->NetError(), response_code,
                             std::move(response_body));
  // Potentially dead after the above invocation; nothing to do except return.
}

void PerUserTopicSubscriptionRequest::OnURLFetchCompleteInternal(
    int net_error,
    int response_code,
    std::unique_ptr<std::string> response_body) {
  if (IsNetworkError(net_error)) {
    RecordRequestStatus(SubscriptionStatus::kNetworkFailure, type_, topic_,
                        net_error, response_code);
    RunCompletedCallbackAndMaybeDie(Status(StatusCode::FAILED, "Network Error"),
                                    std::string());
    // Potentially dead after the above invocation; nothing to do except return.
    return;
  }

  if (response_code != net::HTTP_OK) {
    StatusCode status = StatusCode::FAILED;
    if (response_code == net::HTTP_UNAUTHORIZED) {
      status = StatusCode::AUTH_FAILURE;
    } else if (response_code >= 400 && response_code <= 499) {
      status = StatusCode::FAILED_NON_RETRIABLE;
    }
    RecordRequestStatus(SubscriptionStatus::kHttpFailure, type_, topic_,
                        net_error, response_code);
    RunCompletedCallbackAndMaybeDie(
        Status(status, base::StringPrintf("HTTP Error: %d", response_code)),
        std::string());
    // Potentially dead after the above invocation; nothing to do except return.
    return;
  }

  if (type_ == RequestType::kUnsubscribe) {
    // No response body expected for DELETE requests.
    RecordRequestStatus(SubscriptionStatus::kSuccess, type_, topic_, net_error,
                        response_code);
    RunCompletedCallbackAndMaybeDie(Status(StatusCode::SUCCESS, std::string()),
                                    std::string());
    // Potentially dead after the above invocation; nothing to do except return.
    return;
  }

  if (!response_body || response_body->empty()) {
    RecordRequestStatus(SubscriptionStatus::kParsingFailure, type_, topic_,
                        net_error, response_code);
    RunCompletedCallbackAndMaybeDie(Status(StatusCode::FAILED, "Body missing"),
                                    std::string());
    // Potentially dead after the above invocation; nothing to do except return.
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&PerUserTopicSubscriptionRequest::OnJsonParse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PerUserTopicSubscriptionRequest::OnJsonParse(
    data_decoder::DataDecoder::ValueOrError result) {
  if (const auto topic_name = result.transform(GetTopicName);
      topic_name.has_value() && *topic_name) {
    RecordRequestStatus(SubscriptionStatus::kSuccess, type_, topic_);
    RunCompletedCallbackAndMaybeDie(Status(StatusCode::SUCCESS, std::string()),
                                    **topic_name);
    // Potentially dead after the above invocation; nothing to do except return.
    return;
  }
  RecordRequestStatus(SubscriptionStatus::kParsingFailure, type_, topic_);
  RunCompletedCallbackAndMaybeDie(
      Status(StatusCode::FAILED,
             result.has_value() ? "Missing topic name" : "Body parse error"),
      std::string());
  // Potentially dead after the above invocation; nothing to do except return.
}

void PerUserTopicSubscriptionRequest::RunCompletedCallbackAndMaybeDie(
    Status status,
    std::string topic_name) {
  std::move(request_completed_callback_)
      .Run(std::move(status), std::move(topic_name));
}

PerUserTopicSubscriptionRequest::Builder::Builder() = default;
PerUserTopicSubscriptionRequest::Builder::~Builder() = default;

std::unique_ptr<PerUserTopicSubscriptionRequest>
PerUserTopicSubscriptionRequest::Builder::Build() const {
  DCHECK(!scope_.empty());
  auto request = base::WrapUnique(new PerUserTopicSubscriptionRequest());

  std::string url;
  switch (type_) {
    case RequestType::kSubscribe:
      url = base::StringPrintf(
          "%s/v1/perusertopics/%s/rel/topics/?subscriber_token=%s",
          scope_.c_str(), project_id_.c_str(), instance_id_token_.c_str());
      break;
    case RequestType::kUnsubscribe:
      std::string public_param;
      if (topic_is_public_) {
        public_param = "subscription.is_public=true&";
      }
      url = base::StringPrintf(
          "%s/v1/perusertopics/%s/rel/topics/%s?%ssubscriber_token=%s",
          scope_.c_str(), project_id_.c_str(), topic_.c_str(),
          public_param.c_str(), instance_id_token_.c_str());
      break;
  }
  GURL full_url(url);

  DCHECK(full_url.is_valid());

  request->url_ = full_url;
  request->type_ = type_;
  request->topic_ = topic_;

  std::string body;
  if (type_ == RequestType::kSubscribe) {
    body = BuildBody();
  }
  net::HttpRequestHeaders headers = BuildHeaders();
  request->simple_loader_ = BuildURLFetcher(headers, body, full_url);

  // Log the request for debugging network issues.
  DVLOG(1) << "Building a subscription request to " << full_url << ":\n"
           << headers.ToString() << "\n"
           << body;
  return request;
}

PerUserTopicSubscriptionRequest::Builder&
PerUserTopicSubscriptionRequest::Builder::SetInstanceIdToken(
    const std::string& token) {
  instance_id_token_ = token;
  return *this;
}

PerUserTopicSubscriptionRequest::Builder&
PerUserTopicSubscriptionRequest::Builder::SetScope(const std::string& scope) {
  scope_ = scope;
  return *this;
}

PerUserTopicSubscriptionRequest::Builder&
PerUserTopicSubscriptionRequest::Builder::SetAuthenticationHeader(
    const std::string& auth_header) {
  auth_header_ = auth_header;
  return *this;
}

PerUserTopicSubscriptionRequest::Builder&
PerUserTopicSubscriptionRequest::Builder::SetPublicTopicName(
    const Topic& topic) {
  topic_ = topic;
  return *this;
}

PerUserTopicSubscriptionRequest::Builder&
PerUserTopicSubscriptionRequest::Builder::SetProjectId(
    const std::string& project_id) {
  project_id_ = project_id;
  return *this;
}

PerUserTopicSubscriptionRequest::Builder&
PerUserTopicSubscriptionRequest::Builder::SetType(RequestType type) {
  type_ = type;
  return *this;
}

PerUserTopicSubscriptionRequest::Builder&
PerUserTopicSubscriptionRequest::Builder::SetTopicIsPublic(
    bool topic_is_public) {
  topic_is_public_ = topic_is_public;
  return *this;
}

HttpRequestHeaders PerUserTopicSubscriptionRequest::Builder::BuildHeaders()
    const {
  HttpRequestHeaders headers;
  if (!auth_header_.empty()) {
    headers.SetHeader(HttpRequestHeaders::kAuthorization, auth_header_);
  }
  return headers;
}

std::string PerUserTopicSubscriptionRequest::Builder::BuildBody() const {
  base::Value::Dict request;

  request.Set("public_topic_name", topic_);
  if (topic_is_public_)
    request.Set("is_public", true);

  std::string request_json;
  bool success = base::JSONWriter::Write(request, &request_json);
  DCHECK(success);
  return request_json;
}

std::unique_ptr<network::SimpleURLLoader>
PerUserTopicSubscriptionRequest::Builder::BuildURLFetcher(
    const HttpRequestHeaders& headers,
    const std::string& body,
    const GURL& url) const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("per_user_topic_registration_request",
                                          R"(
        semantics {
          sender:
            "Subscribe for listening to the specific user topic"
          description:
            "This request subscribes the client for receiving FCM messages for"
            "the concrete user topic."
          trigger:
            "Subscription takes place only once per profile per topic. "
          data:
            "An OAuth2 token is sent as an authorization header."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can not be disabled by settings now"
          policy_exception_justification:
            "This feature is required to deliver core user experiences and "
            "cannot be disabled by policy."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  switch (type_) {
    case PerUserTopicSubscriptionRequest::RequestType::kSubscribe:
      request->method = "POST";
      break;
    case PerUserTopicSubscriptionRequest::RequestType::kUnsubscribe:
      request->method = "DELETE";
      break;
  }
  request->url = url;
  request->headers = headers;
  // Disable cookies for this request.
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader->AttachStringForUpload(body, "application/json; charset=UTF-8");

  return url_loader;
}

}  // namespace invalidation
