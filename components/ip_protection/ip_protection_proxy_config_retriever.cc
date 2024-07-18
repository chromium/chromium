// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/ip_protection_proxy_config_retriever.h"

#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "build/branding_buildflags.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ip_protection {

namespace {
constexpr std::string_view kGoogApiKeyHeader = "X-Goog-Api-Key";
constexpr net::NetworkTrafficAnnotationTag kGetProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "ip_protection_service_get_proxy_config",
        R"(
    semantics {
      sender: "Chrome IP Protection Service Client"
      description:
        "Request to a Google auth server to obtain proxy server hostnames "
        "for Chrome's IP Protection privacy proxies."
      trigger:
        "On startup, periodically, and on failure to connect to a proxy."
      data:
        "None"
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "ip-protection-team@google.com"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-08-30"
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
constexpr int kIpProtectionRequestMaxBodySize = 256 * 1024;
constexpr char kProtobufContentType[] = "application/x-protobuf";
}  // namespace

IpProtectionProxyConfigRetriever::IpProtectionProxyConfigRetriever(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string type,
    std::string api_key)
    : url_loader_factory_(std::move(url_loader_factory)),
      ip_protection_server_url_(net::features::kIpPrivacyTokenServer.Get()),
      ip_protection_server_get_proxy_config_path_(
          net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()),
      service_type_(std::move(type)),
      api_key_(std::move(api_key)) {
  CHECK(url_loader_factory_);
}

IpProtectionProxyConfigRetriever::~IpProtectionProxyConfigRetriever() = default;

void IpProtectionProxyConfigRetriever::GetProxyConfig(
    std::optional<std::string> oauth_token,
    GetProxyConfigCallback callback,
    bool for_testing) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!for_testing) {
    std::move(callback).Run(
        base::unexpected("GetProxyConfig is only supported in Chrome builds"));
    return;
  }
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GURL::Replacements replacements;
  replacements.SetPathStr(ip_protection_server_get_proxy_config_path_);
  GURL request_url = ip_protection_server_url_.ReplaceComponents(replacements);
  if (!request_url.is_valid()) {
    std::move(callback).Run(
        base::unexpected("Invalid IP Protection GetProxyConfig URL"));
    return;
  }

  ip_protection::GetProxyConfigRequest get_proxy_config_request;
  get_proxy_config_request.set_service_type(service_type_);

  std::string body;
  get_proxy_config_request.SerializeToString(&body);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(request_url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kProtobufContentType);
  // Set the OAuth token if it's available or otherwise fallback to sending the
  // Google API key.
  if (oauth_token.has_value()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({"Bearer ", oauth_token.value()}));
  } else {
    resource_request->headers.SetHeader(kGoogApiKeyHeader, api_key_);
  }
  int experiment_arm = net::features::kIpPrivacyDebugExperimentArm.Get();
  if (experiment_arm != 0) {
    resource_request->headers.SetHeader("Ip-Protection-Debug-Experiment-Arm",
                                        base::NumberToString(experiment_arm));
  }

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kGetProxyConfigTrafficAnnotation);
  // Retry on network changes, as sometimes this occurs during browser startup.
  url_loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader->AttachStringForUpload(body, kProtobufContentType);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          &IpProtectionProxyConfigRetriever::OnGetProxyConfigCompleted,
          weak_ptr_factory_.GetWeakPtr(),
          // Include the URLLoader in the callback so that it stays
          // alive until the download is complete.
          std::move(url_loader), std::move(callback)),
      kIpProtectionRequestMaxBodySize);
}

void IpProtectionProxyConfigRetriever::OnGetProxyConfigCompleted(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    GetProxyConfigCallback callback,
    std::unique_ptr<std::string> response) {
  if (!response) {
    std::move(callback).Run(base::unexpected("Failed GetProxyConfig request"));
    return;
  }

  ip_protection::GetProxyConfigResponse response_proto;
  if (!response_proto.ParseFromString(*response)) {
    std::move(callback).Run(
        base::unexpected("Failed to parse GetProxyConfig response"));
    return;
  }

  std::move(callback).Run(std::move(response_proto));
}

}  // namespace ip_protection
