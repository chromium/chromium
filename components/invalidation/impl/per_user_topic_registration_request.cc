// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_registration_request.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"

using net::HttpRequestHeaders;

namespace {

const char kPublicTopicNameKey[] = "publicTopicName";
const char kPrivateTopicNameKey[] = "privateTopicName";

const std::string* GetTopicName(const base::Value& value) {
  if (!value.is_dict())
    return nullptr;
  if (value.FindBoolKey("isPublic").value_or(false))
    return value.FindStringKey(kPublicTopicNameKey);
  return value.FindStringKey(kPrivateTopicNameKey);
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

void RecordRequestStatus(
    SubscriptionStatus status,
    syncer::PerUserTopicRegistrationRequest::RequestType type,
    const std::string& topic,
    int net_error = net::OK,
    int response_code = 200) {
  switch (type) {
    case syncer::PerUserTopicRegistrationRequest::SUBSCRIBE: {
      UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.SubscriptionRequestStatus",
                                status);
      break;
    }
    case syncer::PerUserTopicRegistrationRequest::UNSUBSCRIBE: {
      UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.UnsubscriptionRequestStatus",
                                status);
      break;
    }
  }
  if (type != syncer::PerUserTopicRegistrationRequest::SUBSCRIBE) {
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
    base::UmaHistogramSparse(
        "FCMInvalidations.SubscriptionResponseCodeForTopic." + topic,
        response_code);
  }
}

}  // namespace

namespace syncer {

PerUserTopicRegistrationRequest::PerUserTopicRegistrationRequest() {}

PerUserTopicRegistrationRequest::~PerUserTopicRegistrationRequest() = default;

void PerUserTopicRegistrationRequest::Start(
    CompletedCallback callback,
    network::mojom::URLLoaderFactory* loader_factory) {
  DCHECK(request_completed_callback_.is_null()) << "Request already running!";
  request_completed_callback_ = std::move(callback);

  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory,
      base::BindOnce(&PerUserTopicRegistrationRequest::OnURLFetchComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PerUserTopicRegistrationRequest::OnURLFetchComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (simple_loader_->ResponseInfo() &&
      simple_loader_->ResponseInfo()->headers) {
    response_code = simple_loader_->ResponseInfo()->headers->response_code();
  }
  OnURLFetchCompleteInternal(simple_loader_->NetError(), response_code,
                             std::move(response_body));
}

void PerUserTopicRegistrationRequest::OnURLFetchCompleteInternal(
    int net_error,
    int response_code,
    std::unique_ptr<std::string> response_body) {
  if (IsNetworkError(net_error)) {
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::FAILED, base::StringPrintf("Network Error")),
             std::string());
    RecordRequestStatus(SubscriptionStatus::kNetworkFailure, type_, topic_,
                        net_error, response_code);
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
    std::move(request_completed_callback_)
        .Run(
            Status(status, base::StringPrintf("HTTP Error: %d", response_code)),
            std::string());
    return;
  }

  if (type_ == UNSUBSCRIBE) {
    // No response body expected for DELETE requests.
    RecordRequestStatus(SubscriptionStatus::kSuccess, type_, topic_, net_error,
                        response_code);
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::SUCCESS, std::string()), std::string());
    return;
  }

  if (!response_body || response_body->empty()) {
    RecordRequestStatus(SubscriptionStatus::kParsingFailure, type_, topic_,
                        net_error, response_code);
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::FAILED, base::StringPrintf("Body missing")),
             std::string());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&PerUserTopicRegistrationRequest::OnJsonParse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PerUserTopicRegistrationRequest::OnJsonParse(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    RecordRequestStatus(SubscriptionStatus::kParsingFailure, type_, topic_);
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::FAILED, base::StringPrintf("Body parse error")),
             std::string());
    return;
  }

  const std::string* topic_name = GetTopicName(*result.value);
  if (topic_name) {
    RecordRequestStatus(SubscriptionStatus::kSuccess, type_, topic_);
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::SUCCESS, std::string()), *topic_name);
  } else {
    RecordRequestStatus(SubscriptionStatus::kParsingFailure, type_, topic_);
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::FAILED,
                    base::StringPrintf("Missing topic name")),
             std::string());
  }
}

PerUserTopicRegistrationRequest::Builder::Builder() = default;
PerUserTopicRegistrationRequest::Builder::Builder(
    PerUserTopicRegistrationRequest::Builder&&) = default;
PerUserTopicRegistrationRequest::Builder::~Builder() = default;

std::unique_ptr<PerUserTopicRegistrationRequest>
PerUserTopicRegistrationRequest::Builder::Build() const {
  DCHECK(!scope_.empty());
  auto request = base::WrapUnique(new PerUserTopicRegistrationRequest());

  std::string url;
  switch (type_) {
    case SUBSCRIBE:
      url = base::StringPrintf(
          "%s/v1/perusertopics/%s/rel/topics/?subscriber_token=%s",
          scope_.c_str(), project_id_.c_str(), instance_id_token_.c_str());
      break;
    case UNSUBSCRIBE:
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
  if (type_ == SUBSCRIBE)
    body = BuildBody();
  net::HttpRequestHeaders headers = BuildHeaders();
  request->simple_loader_ = BuildURLFetcher(headers, body, full_url);

  // Log the request for debugging network issues.
  DVLOG(1) << "Building a subscription request to " << full_url << ":\n"
           << headers.ToString() << "\n"
           << body;
  return request;
}

PerUserTopicRegistrationRequest::Builder&
PerUserTopicRegistrationRequest::Builder::SetInstanceIdToken(
    const std::string& token) {
  instance_id_token_ = token;
  return *this;
}

PerUserTopicRegistrationRequest::Builder&
PerUserTopicRegistrationRequest::Builder::SetScope(const std::string& scope) {
  scope_ = scope;
  return *this;
}

PerUserTopicRegistrationRequest::Builder&
PerUserTopicRegistrationRequest::Builder::SetAuthenticationHeader(
    const std::string& auth_header) {
  auth_header_ = auth_header;
  return *this;
}

PerUserTopicRegistrationRequest::Builder&
PerUserTopicRegistrationRequest::Builder::SetPublicTopicName(
    const Topic& topic) {
  topic_ = topic;
  return *this;
}

PerUserTopicRegistrationRequest::Builder&
PerUserTopicRegistrationRequest::Builder::SetProjectId(
    const std::string& project_id) {
  project_id_ = project_id;
  return *this;
}

PerUserTopicRegistrationRequest::Builder&
PerUserTopicRegistrationRequest::Builder::SetType(RequestType type) {
  type_ = type;
  return *this;
}

PerUserTopicRegistrationRequest::Builder&
PerUserTopicRegistrationRequest::Builder::SetTopicIsPublic(
    bool topic_is_public) {
  topic_is_public_ = topic_is_public;
  return *this;
}

HttpRequestHeaders PerUserTopicRegistrationRequest::Builder::BuildHeaders()
    const {
  HttpRequestHeaders headers;
  if (!auth_header_.empty()) {
    headers.SetHeader(HttpRequestHeaders::kAuthorization, auth_header_);
  }
  return headers;
}

std::string PerUserTopicRegistrationRequest::Builder::BuildBody() const {
  base::DictionaryValue request;

  request.SetString("public_topic_name", topic_);
  if (topic_is_public_) {
    request.SetBoolean("is_public", true);
  }

  std::string request_json;
  bool success = base::JSONWriter::Write(request, &request_json);
  DCHECK(success);
  return request_json;
}

std::unique_ptr<network::SimpleURLLoader>
PerUserTopicRegistrationRequest::Builder::BuildURLFetcher(
    const HttpRequestHeaders& headers,
    const std::string& body,
    const GURL& url) const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("per_user_topic_registration_request",
                                          R"(
        semantics {
          sender: "Register the Sync client for listening of the specific topic"
          description:
            "Chromium can receive Sync invalidations via FCM messages."
            "This request registers the client for receiving messages for the"
            "concrete topic. In case of Chrome Sync topic is a ModelType,"
            "e.g. BOOKMARK"
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
          chrome_policy: {
             SyncDisabled {
               policy_options {mode: MANDATORY}
               SyncDisabled: false
             }
          }
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  switch (type_) {
    case SUBSCRIBE:
      request->method = "POST";
      break;
    case UNSUBSCRIBE:
      request->method = "DELETE";
      break;
  }
  request->url = url;
  request->headers = headers;
  // TODO(treib): Should we set request->credentials_mode to kOmit, to match
  // "cookies_allowed: NO" above?

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader->AttachStringForUpload(body, "application/json; charset=UTF-8");

  return url_loader;
}

}  // namespace syncer
