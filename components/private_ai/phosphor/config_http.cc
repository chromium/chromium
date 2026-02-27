// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/config_http.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/private_ai/features.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/mutable_network_traffic_annotation_tag_mojom_traits.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace private_ai::phosphor {

namespace {
constexpr net::NetworkTrafficAnnotationTag kGetTokenTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("private_ai_get_token",
                                        R"(
    semantics {
      sender: "PrivateAI Service Client"
      description:
        "Request to a Google auth server to obtain an authorization token "
        "for PrivateAI client attestation."
      trigger:
        "The PrivateAI Service is out of client attestation tokens."
      data:
        "Sign-in OAuth Token"
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "dullweber@chromium.org"
        }
      }
      user_data {
        type: ACCESS_TOKEN
      }
      last_reviewed: "2025-11-15"
    }
    policy {
      cookies_allowed: NO
      policy_exception_justification: "Not implemented."
    }
    comments:
      ""
    )");

// The maximum size of the PrivateAI requests - 256 KB (in practice these
// should be much smaller than this).
const int kPrivateAiRequestMaxBodySize = 256 * 1024;
const char kProtobufContentType[] = "application/x-protobuf";

}  // namespace

ConfigHttp::ConfigHttp(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                           pending_url_loader_factory)
    : pending_url_loader_factory_(std::move(pending_url_loader_factory)) {
  CHECK(pending_url_loader_factory_);
}

ConfigHttp::~ConfigHttp() = default;

// static
GURL ConfigHttp::GetServerUrl() {
  return GURL(kPrivateAiTokenServerUrl.Get());
}

// static
std::string ConfigHttp::GetInitialDataPath() {
  return kPrivateAiTokenServerGetInitialDataPath.Get();
}

// static
std::string ConfigHttp::GetTokensPath() {
  return kPrivateAiTokenServerGetTokensPath.Get();
}

network::SharedURLLoaderFactory* ConfigHttp::GetOrCreateURLLoaderFactory() {
  if (!url_loader_factory_) {
    CHECK(pending_url_loader_factory_);
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_url_loader_factory_));
  }
  return url_loader_factory_.get();
}

void ConfigHttp::DoRequest(quiche::BlindSignMessageRequestType request_type,
                           std::optional<std::string_view> authorization_header,
                           const std::string& body,
                           quiche::BlindSignMessageCallback callback) {
  std::string path;
  switch (request_type) {
    case quiche::BlindSignMessageRequestType::kGetInitialData:
      path = GetInitialDataPath();
      break;
    case quiche::BlindSignMessageRequestType::kAuthAndSign:
      path = GetTokensPath();
      break;
    case quiche::BlindSignMessageRequestType::kAttestAndSign:
    case quiche::BlindSignMessageRequestType::kUnknown:
      std::move(callback)(absl::InternalError("Invalid request type"));
      return;
  }

  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  GURL request_url = GetServerUrl().ReplaceComponents(replacements);
  if (!request_url.is_valid()) {
    std::move(callback)(absl::InternalError("Invalid PrivateAI Token URL"));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(request_url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (!authorization_header.has_value()) {
    std::move(callback)(
        absl::InvalidArgumentError("Missing Authorization header"));
    return;
  }
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", *authorization_header}));
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kProtobufContentType);

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kGetTokenTrafficAnnotation);

  // Retry on network changes.
  url_loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
             network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);

  url_loader->AttachStringForUpload(body, kProtobufContentType);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      GetOrCreateURLLoaderFactory(),
      base::BindOnce(&ConfigHttp::OnDoRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(url_loader), std::move(callback)),
      kPrivateAiRequestMaxBodySize);
}

void ConfigHttp::OnDoRequestCompleted(
    base::TimeTicks start_time,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    quiche::BlindSignMessageCallback callback,
    std::optional<std::string> response) {
  base::UmaHistogramMediumTimes("PrivateAi.Phosphor.ConfigHttp.RequestLatency",
                                base::TimeTicks::Now() - start_time);
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }
  base::UmaHistogramSparse("PrivateAi.Phosphor.ConfigHttp.ResponseCode",
                           response_code);

  // A response code of 0 can be returned if no HTTP response is received, for
  // example in the case of a network error. This is also used by unit tests
  // with mock responses to simulate network failures. This is handled as a
  // generic failure below, so we don't short-circuit here.
  // Short-circuit non-200 HTTP responses to an OK response with that code.
  if (response_code != 200 && response_code != 0) {
    std::move(callback)(quiche::BlindSignMessageResponse(
        quiche::BlindSignMessageResponse::HttpCodeToStatusCode(response_code),
        ""));
    return;
  }

  if (!response.has_value()) {
    std::move(callback)(
        absl::InternalError("Failed Request to Authentication Server"));
    return;
  }

  quiche::BlindSignMessageResponse bsa_response(
      quiche::BlindSignMessageResponse::HttpCodeToStatusCode(response_code),
      std::move(*response));

  std::move(callback)(std::move(bsa_response));
}

}  // namespace private_ai::phosphor
