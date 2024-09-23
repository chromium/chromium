// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/web_push/web_push_sender.h"

#include <limits.h>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/sharing_message/web_push/json_web_token_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

// VAPID header constants.
const char kClaimsKeyAudience[] = "aud";
const char kFCMServerAudience[] = "https://fcm.googleapis.com";

const char kClaimsKeyExpirationTime[] = "exp";
// It's 12 hours rather than 24 hours to avoid any issues with clock differences
// between the sending application and the push service.
constexpr base::TimeDelta kClaimsValidPeriod = base::Hours(12);

const char kAuthorizationRequestHeaderFormat[] = "vapid t=%s, k=%s";

// Endpoint constants.
const char kFCMServerUrlFormat[] = "https://fcm.googleapis.com/fcm/send/%s";

// HTTP header constants.
const char kTTL[] = "TTL";
const char kUrgency[] = "Urgency";

const char kContentEncodingProperty[] = "content-encoding";
const char kContentCodingAes128Gcm[] = "aes128gcm";

// Other constants.
const char kContentEncodingOctetStream[] = "application/octet-stream";

std::optional<std::string> GetAuthHeader(crypto::ECPrivateKey* vapid_key) {
  base::Value::Dict claims;
  claims.Set(kClaimsKeyAudience, base::Value(kFCMServerAudience));

  int64_t exp =
      (base::Time::Now() + kClaimsValidPeriod - base::Time::UnixEpoch())
          .InSeconds();
  // TODO: Year 2038 problem, base::Value does not support int64_t.
  if (exp > INT_MAX) {
    return std::nullopt;
  }

  claims.Set(kClaimsKeyExpirationTime, base::Value(static_cast<int32_t>(exp)));

  std::optional<std::string> jwt = CreateJSONWebToken(claims, vapid_key);
  if (!jwt) {
    return std::nullopt;
  }

  std::string public_key;
  if (!gcm::GetRawPublicKey(*vapid_key, &public_key)) {
    return std::nullopt;
  }

  std::string base64_public_key;
  base::Base64UrlEncode(public_key, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_public_key);

  return base::StringPrintf(kAuthorizationRequestHeaderFormat, jwt->c_str(),
                            base64_public_key.c_str());
}

std::string GetUrgencyHeader(WebPushMessage::Urgency urgency) {
  switch (urgency) {
    case WebPushMessage::Urgency::kVeryLow:
      return "very-low";
    case WebPushMessage::Urgency::kLow:
      return "low";
    case WebPushMessage::Urgency::kNormal:
      return "normal";
    case WebPushMessage::Urgency::kHigh:
      return "high";
  }
}

std::unique_ptr<network::SimpleURLLoader> BuildURLLoader(
    const std::string& fcm_token,
    int time_to_live,
    const std::string& urgency_header,
    const std::string& auth_header,
    const std::string& message) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  std::string server_url =
      base::StringPrintf(kFCMServerUrlFormat, fcm_token.c_str());
  resource_request->url = GURL(server_url);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      auth_header);
  resource_request->headers.SetHeader(kTTL, base::NumberToString(time_to_live));
  resource_request->headers.SetHeader(kContentEncodingProperty,
                                      kContentCodingAes128Gcm);
  resource_request->headers.SetHeader(kUrgency, urgency_header);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("web_push_message", R"(
        semantics {
          sender: "GCMDriver WebPushSender"
          description:
            "Send a request via Firebase to another device that is signed in"
            "with the same account."
          trigger: "Users send data to another owned device."
          data: "Web push message."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: SENSITIVE_URL
            type: OTHER
          }
          internal {
            contacts {
              owners: "//components/sharing_message/OWNERS"
            }
          }
          last_reviewed: "2024-07-16"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature in Chrome's settings under"
            "'Sync and Google services'."
          policy_exception_justification: "Not implemented."
        }
      )");
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  loader->AttachStringForUpload(message, kContentEncodingOctetStream);
  loader->SetRetryOptions(1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  loader->SetAllowHttpErrorResults(true);

  return loader;
}

}  // namespace

WebPushSender::WebPushSender(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

WebPushSender::~WebPushSender() = default;

void WebPushSender::SendMessage(const std::string& fcm_token,
                                crypto::ECPrivateKey* vapid_key,
                                WebPushMessage message,
                                WebPushCallback callback) {
  DCHECK(!fcm_token.empty());
  DCHECK(vapid_key);
  DCHECK_LE(message.time_to_live, message.kMaximumTTL);

  std::optional<std::string> auth_header = GetAuthHeader(vapid_key);
  if (!auth_header) {
    DLOG(ERROR) << "Failed to create JWT";
    InvokeWebPushCallback(std::move(callback),
                          SendWebPushMessageResult::kCreateJWTFailed);
    return;
  }

  std::unique_ptr<network::SimpleURLLoader> url_loader = BuildURLLoader(
      fcm_token, message.time_to_live, GetUrgencyHeader(message.urgency),
      *auth_header, message.payload);
  network::SimpleURLLoader* const url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&WebPushSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)),
      message.payload.size());
}

void WebPushSender::OnMessageSent(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    WebPushCallback callback,
    std::unique_ptr<std::string> response_body) {
  int net_error = url_loader->NetError();
  if (net_error != net::OK) {
    if (net_error == net::ERR_INSUFFICIENT_RESOURCES) {
      DLOG(ERROR) << "VAPID key invalid";
      InvokeWebPushCallback(std::move(callback),
                            SendWebPushMessageResult::kVapidKeyInvalid);
    } else {
      DLOG(ERROR) << "Network Error: " << net_error;
      InvokeWebPushCallback(std::move(callback),
                            SendWebPushMessageResult::kNetworkError);
    }
    return;
  }

  if (!url_loader->ResponseInfo() || !url_loader->ResponseInfo()->headers) {
    DLOG(ERROR) << "Response info not found";
    InvokeWebPushCallback(std::move(callback),
                          SendWebPushMessageResult::kServerError);
    return;
  }

  scoped_refptr<net::HttpResponseHeaders> response_headers =
      url_loader->ResponseInfo()->headers;
  int response_code = response_headers->response_code();
  if (response_code == net::HTTP_NOT_FOUND || response_code == net::HTTP_GONE) {
    DLOG(ERROR) << "Device no longer registered";
    InvokeWebPushCallback(std::move(callback),
                          SendWebPushMessageResult::kDeviceGone);
    return;
  }

  // Note: FCM is not following spec and returns 400 for payload too large.
  if (response_code == net::HTTP_REQUEST_ENTITY_TOO_LARGE ||
      response_code == net::HTTP_BAD_REQUEST) {
    DLOG(ERROR) << "Payload too large";
    InvokeWebPushCallback(std::move(callback),
                          SendWebPushMessageResult::kPayloadTooLarge);
    return;
  }

  if (!network::IsSuccessfulStatus(response_code)) {
    DLOG(ERROR) << "HTTP Error: " << response_code;
    InvokeWebPushCallback(std::move(callback),
                          SendWebPushMessageResult::kServerError);
    return;
  }

  std::string location;
  if (!response_headers->EnumerateHeader(nullptr, "location", &location)) {
    DLOG(ERROR) << "Failed to get location header from response";
    InvokeWebPushCallback(std::move(callback),
                          SendWebPushMessageResult::kParseResponseFailed);
    return;
  }

  size_t slash_pos = location.rfind("/");
  if (slash_pos == std::string::npos) {
    DLOG(ERROR) << "Failed to parse message_id from location header";
    InvokeWebPushCallback(std::move(callback),
                          SendWebPushMessageResult::kParseResponseFailed);
    return;
  }

  InvokeWebPushCallback(std::move(callback),
                        SendWebPushMessageResult::kSuccessful,
                        /*message_id=*/location.substr(slash_pos + 1));
}
