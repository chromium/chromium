// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_config_http.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "net/base/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ip_protection {

namespace {
constexpr net::NetworkTrafficAnnotationTag kGetTokenTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ip_protection_service_get_token",
                                        R"(
    semantics {
      sender: "IP Protection Service Client"
      description:
        "Request to a Google auth server to obtain an authorization token "
        "for IP Protection privacy proxies."
      trigger:
        "The IP Protection Service is out of proxy authorization tokens."
      data:
        "Sign-in OAuth Token"
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "ip-protection-team@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
      }
      last_reviewed: "2024-09-26"
    }
    policy {
      cookies_allowed: NO
      policy_exception_justification: "Not implemented."
    }
    comments:
      ""
    )");

// The maximum size of the IP Protection requests - 256 KB (in practice these
// should be much smaller than this).
const int kIpProtectionRequestMaxBodySize = 256 * 1024;
const char kProtobufContentType[] = "application/x-protobuf";

}  // namespace

IpProtectionConfigHttp::IpProtectionConfigHttp(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      ip_protection_server_url_(net::features::kIpPrivacyTokenServer.Get()),
      ip_protection_server_get_initial_data_path_(
          net::features::kIpPrivacyTokenServerGetInitialDataPath.Get()),
      ip_protection_server_get_tokens_path_(
          net::features::kIpPrivacyTokenServerGetTokensPath.Get()) {
  CHECK(url_loader_factory_);
}

IpProtectionConfigHttp::~IpProtectionConfigHttp() = default;

void IpProtectionConfigHttp::DoRequest(
    quiche::BlindSignMessageRequestType request_type,
    std::optional<std::string_view> authorization_header,
    const std::string& body,
    quiche::BlindSignMessageCallback callback) {
  GURL::Replacements replacements;
  switch (request_type) {
    case quiche::BlindSignMessageRequestType::kGetInitialData:
      replacements.SetPathStr(ip_protection_server_get_initial_data_path_);
      break;
    case quiche::BlindSignMessageRequestType::kAuthAndSign:
      replacements.SetPathStr(ip_protection_server_get_tokens_path_);
      break;
    case quiche::BlindSignMessageRequestType::kUnknown:
      NOTREACHED();
  }

  GURL request_url = ip_protection_server_url_.ReplaceComponents(replacements);
  if (!request_url.is_valid()) {
    std::move(callback)(absl::InternalError("Invalid IP Protection Token URL"));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(request_url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  CHECK(authorization_header.has_value());
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", *authorization_header}));
  int experiment_arm = net::features::kIpPrivacyDebugExperimentArm.Get();
  if (experiment_arm != 0) {
    resource_request->headers.SetHeader("Ip-Protection-Debug-Experiment-Arm",
                                        base::NumberToString(experiment_arm));
  }
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kProtobufContentType);

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kGetTokenTrafficAnnotation);

  // Retry on network changes, for consistency with GetProxyConfig requests.
  url_loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader->AttachStringForUpload(body, kProtobufContentType);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IpProtectionConfigHttp::OnDoRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)),
      kIpProtectionRequestMaxBodySize);
}

void IpProtectionConfigHttp::OnDoRequestCompleted(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    quiche::BlindSignMessageCallback callback,
    std::unique_ptr<std::string> response) {
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  // Short-circuit non-200 HTTP responses to an OK response with that code.
  if (response_code != 200 && response_code != 0) {
    std::move(callback)(quiche::BlindSignMessageResponse(
        quiche::BlindSignMessageResponse::HttpCodeToStatusCode(response_code),
        ""));
    return;
  }

  if (!response) {
    std::move(callback)(
        absl::InternalError("Failed Request to Authentication Server"));
    return;
  }

  quiche::BlindSignMessageResponse bsa_response(
      quiche::BlindSignMessageResponse::HttpCodeToStatusCode(response_code),
      std::move(*response));

  std::move(callback)(std::move(bsa_response));
}

}  // namespace ip_protection
