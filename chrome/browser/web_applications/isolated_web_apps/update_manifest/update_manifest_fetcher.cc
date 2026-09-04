// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "net/base/address_list.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// We need to artificially limit the size of the update manifest, because it is
// loaded into memory.
constexpr size_t kMaxUpdateManifestLength = 5 * 1024 * 1024;

}  // namespace

// static
std::string_view UpdateManifestFetcher::ErrorToString(Error error) {
  switch (error) {
    case Error::kDownloadFailed:
      return "Failed to download update manifest";
    case Error::kInvalidJson:
      return "Update manifest contains invalid JSON";
    case Error::kInvalidManifest:
      return "Invalid update manifest format";
  }
}

UpdateManifestFetcher::UpdateManifestFetcher(
    GURL url,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::NetworkContext* network_context,
    bool report_histogram_manifest_result)
    : url_(std::move(url)),
      partial_traffic_annotation_(std::move(partial_traffic_annotation)),
      url_loader_factory_(std::move(url_loader_factory)),
      report_histogram_manifest_result_(report_histogram_manifest_result),
      host_resolver_(network::SimpleHostResolver::Create(network_context)) {}

UpdateManifestFetcher::~UpdateManifestFetcher() = default;

void UpdateManifestFetcher::FetchUpdateManifest(FetchCallback fetch_callback) {
  CHECK(!fetch_callback_);
  fetch_callback_ = std::move(fetch_callback);

  if (auto space = network::GetAddressSpaceFromUrl(url_)) {
    DownloadUpdateManifest(*space);
    return;
  }

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->initial_priority = net::RequestPriority::HIGHEST;

  host_resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair::FromURL(url_)),
      net::NetworkAnonymizationKey(), std::move(parameters),
      base::BindOnce(&UpdateManifestFetcher::OnHostResolved,
                     weak_factory_.GetWeakPtr()));
}

void UpdateManifestFetcher::OnHostResolved(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const net::AddressList& resolved_addresses,
    const net::HostResolverEndpointResults& alternative_endpoints) {
  network::mojom::IPAddressSpace space =
      network::mojom::IPAddressSpace::kUnknown;
  if (result == net::OK && !resolved_addresses.empty()) {
    // If resolved_addresses contains multiple addresses with different address
    // spaces (e.g. both public and private IPs), using
    // resolved_addresses.front() might lead to a security bypass if the network
    // service ends up connecting to a public IP but we set the client security
    // state to kLocal (based on the first address being private). To be safe,
    // we iterate over all resolved addresses and choose the 'most public'
    // (least privileged) address space (i.e. kPublic > kLocal > kLoopback).
    space = network::IPAddressToIPAddressSpace(
        std::ranges::max(resolved_addresses, network::IsLessPublicAddressSpace,
                         [](const auto& endpoint) {
                           return network::IPAddressToIPAddressSpace(
                               endpoint.address());
                         })
            .address());
  }

  DownloadUpdateManifest(space);
}

void UpdateManifestFetcher::DownloadUpdateManifest(
    network::mojom::IPAddressSpace client_space) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("iwa_update_manifest_fetcher",
                                            partial_traffic_annotation_,
                                            R"(
    semantics {
      data:
        "This request does not send any user data. Its destination is the URL "
        "of an Update Manifest for an Isolated Web App that is installed for "
        "the user."
      destination: OTHER
      internal {
        contacts {
          owners: "//chrome/browser/web_applications/isolated_web_apps/OWNERS"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-05-25"
    }
    policy {
      cookies_allowed: NO
    })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Configure the security state for this request to enable Local Network
  // Access (LNA) checks. By specifying the 'client_space' (determined during
  // host resolution), we allow the network service to identify if this is a
  // request from a more privileged space to a less privileged one (e.g., Public
  // -> Local) and enforce the appropriate security policies, such as CORS
  // preflights for LNA.
  auto client_security_state = network::mojom::ClientSecurityState::New();
  client_security_state->ip_address_space = client_space;
  client_security_state->is_web_secure_context = true;
  client_security_state->local_network_access_request_policy =
      network::mojom::LocalNetworkAccessRequestPolicy::kBlock;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->client_security_state =
      std::move(client_security_state);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), std::move(traffic_annotation));

  simple_url_loader_->SetRetryOptions(
      /* max_retries=*/3,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&UpdateManifestFetcher::OnUpdateManifestDownloaded,
                     weak_factory_.GetWeakPtr()),
      kMaxUpdateManifestLength);
}

void UpdateManifestFetcher::OnUpdateManifestDownloaded(
    std::optional<std::string> update_manifest_content) {
  if (report_histogram_manifest_result_) {
    int error_or_http_response_code_ = 0;
    if (simple_url_loader_->NetError() != net::OK &&
        simple_url_loader_->NetError() != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
      error_or_http_response_code_ = simple_url_loader_->NetError();
    } else {
      // According to the SimpleURLLoader contract,
      // when net_error is net::OK or net::ERR_HTTP_RESPONSE_CODE_FAILURE.
      // simple_url_loader_->ResponseInfo->headers should exist.
      CHECK(simple_url_loader_->ResponseInfo());
      CHECK(simple_url_loader_->ResponseInfo()->headers);
      error_or_http_response_code_ =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    base::UmaHistogramSparse(
        "WebApp.Isolated.UpdateManifest.HttpResponseOrErrorCode",
        error_or_http_response_code_);
  }

  simple_url_loader_.reset();

  if (!update_manifest_content) {
    std::move(fetch_callback_).Run(base::unexpected(Error::kDownloadFailed));
    return;
  }

  ParseUpdateManifest(*update_manifest_content);
}

void UpdateManifestFetcher::ParseUpdateManifest(
    const std::string& update_manifest_content) {
  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(update_manifest_content,
                                                    base::JSON_PARSE_RFC);

  if (!result.has_value()) {
    LOG(ERROR) << "Unable to parse IWA Update Manifest JSON for URL " << url_
               << ". Error was: " << result.error().message;
    std::move(fetch_callback_).Run(base::unexpected(Error::kInvalidJson));
    return;
  }

  base::expected<UpdateManifest, UpdateManifest::JsonFormatError>
      update_manifest =
          UpdateManifest::CreateFromJson(std::move(*result), url_);

  std::move(fetch_callback_)
      .Run(update_manifest.transform_error(
          [this](UpdateManifest::JsonFormatError error) -> Error {
            DVLOG(1) << "Unable to parse IWA Update Manifest format for URL "
                     << url_ << ": " << UpdateManifest::ErrorToString(error);
            return Error::kInvalidManifest;
          }));
}

}  // namespace web_app
