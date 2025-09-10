// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/time/time.h"
#include "url/gurl.h"

namespace ip_protection {

namespace {

constexpr net::NetworkTrafficAnnotationTag kGetProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "ip_protection_service_get_proxy_config",
        R"(
    semantics {
      sender: "IP Protection Service Client"
      description:
        "Request to a Google auth server to obtain proxy server hostnames "
        "for IP Protection privacy proxies."
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
constexpr int kIpProtectionRequestMaxBodySize = 256 * 1024;
constexpr char kProtobufContentType[] = "application/x-protobuf";

}  // namespace

IpProtectionProxyConfigDirectFetcher::IpProtectionProxyConfigDirectFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string service_type,
    Delegate* delegate)
    : delegate_(delegate) {
  CHECK(url_loader_factory);
  ip_protection_proxy_config_retriever_ =
      std::make_unique<IpProtectionProxyConfigDirectFetcher::Retriever>(
          url_loader_factory, service_type, delegate);
}

IpProtectionProxyConfigDirectFetcher::IpProtectionProxyConfigDirectFetcher(
    std::unique_ptr<Retriever> retriever,
    Delegate* delegate)
    : delegate_(delegate) {
  ip_protection_proxy_config_retriever_ = std::move(retriever);
}

IpProtectionProxyConfigDirectFetcher::~IpProtectionProxyConfigDirectFetcher() =
    default;

void IpProtectionProxyConfigDirectFetcher::GetProxyConfig(
    GetProxyConfigCallback callback) {
  // If IP Protection is disabled via user settings then don't attempt to get a
  // proxy list.
  // TODO(crbug.com/41494110): Ideally the enabled/disabled status would be
  // handled above the level of the fetcher (in managers or core), but for the
  // moment it is handled here.
  if (!delegate_->IsProxyConfigFetchEnabled()) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  // If we are not able to call `RetrieveProxyConfig` yet, return early.
  if (no_get_proxy_config_until_ > base::Time::Now()) {
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  ip_protection_proxy_config_retriever_->RetrieveProxyConfig(base::BindOnce(
      &IpProtectionProxyConfigDirectFetcher::OnGetProxyConfigCompleted,
      base::Unretained(this), std::move(callback)));
}

void IpProtectionProxyConfigDirectFetcher::ClearBackoffTimer() {
  no_get_proxy_config_until_ = base::Time();
  next_get_proxy_config_backoff_ = kGetProxyConfigFailureTimeout;
}

void IpProtectionProxyConfigDirectFetcher::OnGetProxyConfigCompleted(
    GetProxyConfigCallback callback,
    base::expected<GetProxyConfigResponse, std::string> response) {
  // If either there is an empty response or no geo hint present, it should be
  // treated as an error and cause a retry.
  if (IsProxyConfigResponseError(response)) {
    VLOG(2)
        << "IpProtectionProxyConfigDirectFetcher::CallGetProxyConfig failed: "
        << response.error();

    // Apply exponential backoff to this sort of failure.
    no_get_proxy_config_until_ =
        base::Time::Now() + next_get_proxy_config_backoff_;
    next_get_proxy_config_backoff_ *= 2;

    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }

  // Cancel any backoff on success.
  ClearBackoffTimer();

  std::vector<net::ProxyChain> proxy_list =
      GetProxyListFromProxyConfigResponse(response.value());
  std::optional<GeoHint> geo_hint =
      GetGeoHintFromProxyConfigResponse(response.value());
  std::move(callback).Run(std::move(proxy_list), std::move(geo_hint));
}

bool IpProtectionProxyConfigDirectFetcher::IsProxyConfigResponseError(
    const base::expected<GetProxyConfigResponse, std::string>& response) {
  if (!response.has_value()) {
    return true;
  }

  // Returns true for an error when a geo hint is missing but is required b/c
  // the proxy chain is NOT empty.
  const GetProxyConfigResponse& config_response = response.value();
  return !config_response.has_geo_hint() &&
         !config_response.proxy_chain().empty();
}

std::vector<net::ProxyChain>
IpProtectionProxyConfigDirectFetcher::GetProxyListFromProxyConfigResponse(
    GetProxyConfigResponse response) {
  // Shortcut to create a ProxyServer with SCHEME_HTTPS from a string in the
  // proto.
  auto add_server = [](std::vector<net::ProxyServer>& proxies,
                       std::string host) {
    net::ProxyServer proxy_server = net::ProxySchemeHostAndPortToProxyServer(
        net::ProxyServer::SCHEME_HTTPS, host);
    if (!proxy_server.is_valid()) {
      return false;
    }
    proxies.push_back(proxy_server);
    return true;
  };

  std::vector<net::ProxyChain> proxy_list;
  for (const auto& proxy_chain : response.proxy_chain()) {
    std::vector<net::ProxyServer> proxies;
    bool ok = true;
    bool overridden = false;
    if (const std::string a_override =
            net::features::kIpPrivacyProxyAHostnameOverride.Get();
        a_override != "") {
      overridden = true;
      ok = ok && add_server(proxies, a_override);
    } else {
      ok = ok && add_server(proxies, proxy_chain.proxy_a());
    }
    if (const std::string b_override =
            net::features::kIpPrivacyProxyBHostnameOverride.Get();
        ok && b_override != "") {
      overridden = true;
      ok = ok && add_server(proxies, b_override);
    } else {
      ok = ok && add_server(proxies, proxy_chain.proxy_b());
    }

    // Create a new ProxyChain if the proxies were all valid.
    if (ok) {
      // If the `chain_id` is out of range or local features overrode the
      // chain, use the proxy chain anyway, but with the default `chain_id`.
      // This allows adding new IDs on the server side without breaking older
      // browsers.
      int chain_id = proxy_chain.chain_id();
      if (overridden || chain_id < 0 ||
          chain_id > net::ProxyChain::kMaxIpProtectionChainId) {
        chain_id = net::ProxyChain::kDefaultIpProtectionChainId;
      }
      auto chain =
          net::ProxyChain::ForIpProtection(std::move(proxies), chain_id);
      // `add_server()` fails if the proxy server is invalid, and `ok` will be
      // false in that case, so it's safe to assume the chain we create here
      // will be valid.
      CHECK(chain.IsValid());
      proxy_list.push_back(std::move(chain));
    }
  }

  // Log is consumed by E2E tests. Please CC potassium-engprod@google.com if you
  // have to change this log.
  VLOG(2) << "IPATP::GetProxyList got proxy list of length "
          << proxy_list.size();

  return proxy_list;
}

std::optional<GeoHint>
IpProtectionProxyConfigDirectFetcher::GetGeoHintFromProxyConfigResponse(
    GetProxyConfigResponse& response) {
  if (!response.has_geo_hint()) {
    return std::nullopt;  // No GeoHint available in the response.
  }

  return std::make_optional<GeoHint>(
      {.country_code = response.geo_hint().country_code(),
       .iso_region = response.geo_hint().iso_region(),
       .city_name = response.geo_hint().city_name()});
}

net::ProxyChain IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
    std::vector<std::string> hostnames,
    int chain_id) {
  std::vector<net::ProxyServer> servers;
  for (auto& hostname : hostnames) {
    servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
        net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
  }
  auto chain = net::ProxyChain::ForIpProtection(servers, chain_id);
  CHECK(chain.IsValid());
  return chain;
}

IpProtectionProxyConfigDirectFetcher::Retriever::Retriever(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string service_type,
    Delegate* delegate)
    : url_loader_factory_(std::move(url_loader_factory)),
      ip_protection_server_url_(net::features::kIpPrivacyTokenServer.Get()),
      ip_protection_server_get_proxy_config_path_(
          net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()),
      service_type_(std::move(service_type)),
      delegate_(delegate) {
  CHECK(url_loader_factory_);
}

IpProtectionProxyConfigDirectFetcher::Retriever::~Retriever() = default;

void IpProtectionProxyConfigDirectFetcher::Retriever::RetrieveProxyConfig(
    RetrieveCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  if (delegate_) {
    delegate_->AuthenticateRequest(
        std::move(resource_request),
        base::BindOnce(&IpProtectionProxyConfigDirectFetcher::Retriever::
                           OnAuthenticatedResourceRequest,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    OnAuthenticatedResourceRequest(std::move(callback), true,
                                   std::move(resource_request));
  }
}

void IpProtectionProxyConfigDirectFetcher::Retriever::
    OnAuthenticatedResourceRequest(
        RetrieveCallback callback,
        bool success,
        std::unique_ptr<network::ResourceRequest> resource_request) {
  if (!success) {
    std::move(callback).Run(base::unexpected("Failed to authenticate request"));
    return;
  }
  GURL::Replacements replacements;
  replacements.SetPathStr(ip_protection_server_get_proxy_config_path_);
  GURL request_url = ip_protection_server_url_.ReplaceComponents(replacements);
  if (!request_url.is_valid()) {
    std::move(callback).Run(
        base::unexpected("Invalid IP Protection GetProxyConfig URL"));
    return;
  }

  GetProxyConfigRequest get_proxy_config_request;
  get_proxy_config_request.set_service_type(service_type_);

  std::string body;
  get_proxy_config_request.SerializeToString(&body);

  resource_request->url = std::move(request_url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kProtobufContentType);
  int experiment_arm = net::features::kIpPrivacyDebugExperimentArm.Get();
  if (experiment_arm != 0) {
    resource_request->headers.SetHeader("Ip-Protection-Debug-Experiment-Arm",
                                        base::NumberToString(experiment_arm));
  }

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kGetProxyConfigTrafficAnnotation);
  // Retry on network changes, as sometimes this occurs during browser startup.
  // A network change during DNS resolution results in a DNS error rather than
  // a network change error, so retry in those cases as well.
  url_loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
             network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);

  url_loader->AttachStringForUpload(body, kProtobufContentType);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IpProtectionProxyConfigDirectFetcher::Retriever::
                         OnGetProxyConfigCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Include the URLLoader in the callback so that it stays
                     // alive until the download is complete.
                     std::move(url_loader), std::move(callback)),
      kIpProtectionRequestMaxBodySize);
}

void IpProtectionProxyConfigDirectFetcher::Retriever::OnGetProxyConfigCompleted(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    RetrieveCallback callback,
    std::optional<std::string> response) {
  if (!response) {
    std::move(callback).Run(base::unexpected("Failed GetProxyConfig request"));
    return;
  }

  GetProxyConfigResponse response_proto;
  if (!response_proto.ParseFromString(*response)) {
    std::move(callback).Run(
        base::unexpected("Failed to parse GetProxyConfig response"));
    return;
  }

  std::move(callback).Run(std::move(response_proto));
}

void IpProtectionProxyConfigDirectFetcher::AccountStatusChanged(
    bool account_available) {
  if (account_available) {
    ClearBackoffTimer();
  }
}
}  // namespace ip_protection
