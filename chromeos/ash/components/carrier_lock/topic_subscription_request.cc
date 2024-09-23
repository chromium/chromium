// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/topic_subscription_request.h"

#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "google_apis/credentials_mode.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash::carrier_lock {

namespace {

const char kTopicSubscriptionRequestContentType[] =
    "application/x-www-form-urlencoded";
const char kFcmRegistrationUrl[] =
    "https://android.clients.google.com/c2dm/register3";

// Request constants.
const char kAppKey[] = "app";
const char kSenderKey[] = "sender";
const char kDeviceKey[] = "device";
const char kTopicKey[] = "X-gcm.topic";
const char kDeleteKey[] = "delete";
const char kLoginHeader[] = "AidLogin";

// Response constants.
const char kErrorPrefix[] = "Error=";
const char kAuthenticationFailed[] = "AUTHENTICATION_FAILED";
const char kInvalidSender[] = "INVALID_SENDER";
const char kInvalidParameters[] = "INVALID_PARAMETERS";
const char kInternalServerError[] = "InternalServerError";
const char kQuotaExceeded[] = "QUOTA_EXCEEDED";
const char kTooManySubscribers[] = "TOO_MANY_SUBSCRIBERS";

// Gets correct status from the error message.
Result GetStatusFromError(std::string& error) {
  if (error.find(kAuthenticationFailed) != std::string::npos) {
    error = kAuthenticationFailed;
    return Result::kInvalidInput;
  }
  if (error.find(kInvalidSender) != std::string::npos) {
    error = kInvalidSender;
    return Result::kInvalidInput;
  }
  if (error.find(kInvalidParameters) != std::string::npos) {
    error = kInvalidParameters;
    return Result::kInvalidInput;
  }
  if (error.find(kInternalServerError) != std::string::npos) {
    error = kInternalServerError;
    return Result::kServerInternalError;
  }
  if (error.find(kQuotaExceeded) != std::string::npos) {
    error = kQuotaExceeded;
    return Result::kServerInternalError;
  }
  if (error.find(kTooManySubscribers) != std::string::npos) {
    error = kTooManySubscribers;
    return Result::kServerInternalError;
  }

  // Handle unknown errors.
  size_t pos = std::size(kErrorPrefix);
  error = error.substr(pos, error.size() - pos);
  return Result::kRequestFailed;
}

// Create encoding: "key=value" and append it to "out" string
void BuildFormEncoding(const std::string& key,
                       const std::string& value,
                       std::string* out) {
  if (!out->empty()) {
    out->append("&");
  }
  out->append(key + "=" + base::EscapeUrlEncodedData(value, true));
}

}  // namespace

TopicSubscriptionRequest::RequestInfo::RequestInfo(uint64_t android_id,
                                                   uint64_t security_token,
                                                   const std::string& app_id,
                                                   const std::string& token,
                                                   const std::string& topic,
                                                   bool unsubscribe)
    : android_id(android_id),
      security_token(security_token),
      app_id(app_id),
      token(token),
      topic(topic),
      unsubscribe(unsubscribe) {
  DCHECK(android_id != 0UL);
  DCHECK(security_token != 0UL);
  DCHECK(!app_id.empty());
  DCHECK(!token.empty());
  DCHECK(!topic.empty());
}

TopicSubscriptionRequest::RequestInfo::RequestInfo(const RequestInfo&) =
    default;

TopicSubscriptionRequest::RequestInfo::~RequestInfo() = default;

TopicSubscriptionRequest::TopicSubscriptionRequest(
    const RequestInfo& request_info,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Callback callback)
    : request_callback_(std::move(callback)),
      request_info_(request_info),
      url_loader_factory_(std::move(url_loader_factory)) {
  topic_subscription_url_ = GURL(kFcmRegistrationUrl);
}

TopicSubscriptionRequest::~TopicSubscriptionRequest() = default;

void TopicSubscriptionRequest::Start() {
  DCHECK(!request_callback_.is_null());
  DCHECK(!url_loader_.get());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("carrier_lock_manager_fcm_topic", R"(
        semantics {
          sender: "Carrier Lock manager"
          description:
            "Carrier Lock Manager subscribes to public topics on FCM"
            "to receive push notifications in case of lock changes."
          trigger: "This request happens once on every boot if the device"
                   "has carrier lock enabled."
          data:
            "The topic name and a registration token."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
                email: "cros-cellular-core@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2023-10-24"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Carrier Lock is always enforced."
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = topic_subscription_url_;
  request->method = "POST";
  request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();

  request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      std::string(kLoginHeader) + " " +
          base::NumberToString(request_info_.android_id) + ":" +
          base::NumberToString(request_info_.security_token));

  std::string body;
  BuildFormEncoding(kDeviceKey, base::NumberToString(request_info_.android_id),
                    &body);
  BuildFormEncoding(kAppKey, request_info_.app_id, &body);
  BuildFormEncoding(kSenderKey, request_info_.token, &body);
  BuildFormEncoding(kTopicKey, request_info_.topic, &body);
  if (request_info_.unsubscribe) {
    BuildFormEncoding(kDeleteKey, "true", &body);
  }

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->AttachStringForUpload(body,
                                     kTopicSubscriptionRequestContentType);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&TopicSubscriptionRequest::OnUrlLoadComplete,
                     base::Unretained(this), url_loader_.get()));
}

void TopicSubscriptionRequest::OnUrlLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body) {
  if (source->NetError() != net::OK) {
    LOG(ERROR) << "Failed to fetch URL.";
    ReturnResult(Result::kConnectionError);
    return;
  }

  std::string response;
  if (!body) {
    LOG(ERROR) << "Failed to get response.";
    ReturnResult(Result::kConnectionError);
    return;
  }
  response = std::move(*body);

  // If we are able to parse a meaningful known error, let's do so. Note that
  // some errors will have HTTP_OK response code!
  size_t error_pos = response.find(kErrorPrefix);
  if (error_pos != std::string::npos) {
    std::string error = response.substr(error_pos);
    ReturnResult(GetStatusFromError(error));
    LOG(ERROR) << "Received error in response: " << error;
    return;
  }

  // Can't even get any header info.
  if (!source->ResponseInfo() || !source->ResponseInfo()->headers) {
    LOG(ERROR) << "Missing header in response.";
    ReturnResult(Result::kInvalidResponse);
    return;
  }

  // If we cannot tell what the error is, but at least we know response code was
  // not OK.
  if (source->ResponseInfo()->headers->response_code() != net::HTTP_OK) {
    LOG(ERROR) << "HTTP response code not OK: "
               << source->ResponseInfo()->headers->response_code();
    ReturnResult(Result::kInvalidResponse);
    return;
  }

  ReturnResult(Result::kSuccess);
}

void TopicSubscriptionRequest::ReturnResult(Result result) {
  std::move(request_callback_).Run(result);
}

}  // namespace ash::carrier_lock
