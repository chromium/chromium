// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/net/network_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "components/update_client/net/network_chromium.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

const net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("update_client", R"(
        semantics {
          sender: "Component Updater and Extension Updater"
          description:
            "This network module is used by both the component and the "
            "extension updaters in Chrome. "
            "The component updater is responsible for updating code and data "
            "modules such as Flash, CrlSet, Origin Trials, etc. These modules "
            "are updated on cycles independent of the Chrome release tracks. "
            "It runs in the browser process and communicates with a set of "
            "servers using the Omaha protocol to find the latest versions of "
            "components, download them, and register them with the rest of "
            "Chrome. "
            "The extension updater works similarly, but it updates user "
            "extensions instead of Chrome components. "
          trigger: "Manual or automatic software updates."
          data:
            "Various OS and Chrome parameters such as version, bitness, "
            "release tracks, etc. The component and the extension ids are also "
            "present in both the request and the response from the servers. "
            "The URL that refers to a component CRX payload is obfuscated for "
            "most components."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled."
          chrome_policy {
            ComponentUpdatesEnabled {
              policy_options {mode: MANDATORY}
              ComponentUpdatesEnabled: false
            }
          }
        })");

// Returns the string value of a header of the server response or an empty
// string if the header is not available. Only the first header is returned
// if multiple instances of the same header are present.
std::string GetStringHeader(const network::SimpleURLLoader* simple_url_loader,
                            const char* header_name) {
  CHECK(simple_url_loader);

  const auto* response_info = simple_url_loader->ResponseInfo();
  if (!response_info || !response_info->headers) {
    return {};
  }

  std::string header_value;
  return response_info->headers->EnumerateHeader(nullptr, header_name,
                                                 &header_value)
             ? header_value
             : std::string{};
}

// Returns the integral value of a header of the server response or -1 if
// if the header is not available or a conversion error has occured.
int64_t GetInt64Header(const network::SimpleURLLoader* simple_url_loader,
                       const char* header_name) {
  CHECK(simple_url_loader);

  const auto* response_info = simple_url_loader->ResponseInfo();
  if (!response_info || !response_info->headers) {
    return -1;
  }

  return response_info->headers->GetInt64HeaderValue(header_name);
}

}  // namespace

namespace update_client {

NetworkFetcherImpl::NetworkFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory,
    SendCookiesPredicate cookie_predicate)
    : shared_url_network_factory_(shared_url_network_factory),
      cookie_predicate_(cookie_predicate) {}
NetworkFetcherImpl::~NetworkFetcherImpl() = default;

void NetworkFetcherImpl::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  for (const auto& [name, value] : post_additional_headers) {
    resource_request->headers.SetHeader(name, value);
  }
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_url_loader->SetRetryOptions(
      kMaxRetriesOnNetworkChange,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // The `Content-Type` header set by |AttachStringForUpload| overwrites any
  // `Content-Type` header present in the |ResourceRequest| above.
  simple_url_loader->AttachStringForUpload(post_data, content_type);
  simple_url_loader->SetOnResponseStartedCallback(base::BindOnce(
      &NetworkFetcherImpl::OnResponseStartedCallback, base::Unretained(this),
      std::move(response_started_callback)));
  simple_url_loader->SetOnDownloadProgressCallback(base::BindRepeating(
      &NetworkFetcherImpl::OnProgressCallback, base::Unretained(this),
      std::move(progress_callback)));
  constexpr size_t kMaxResponseSize = 1024 * 1024;
  simple_url_loader->DownloadToString(
      shared_url_network_factory_.get(),
      base::BindOnce(
          [](std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
             PostRequestCompleteCallback post_request_complete_callback,
             std::unique_ptr<std::string> response_body) {
            std::move(post_request_complete_callback)
                .Run(std::move(response_body), simple_url_loader->NetError(),
                     GetStringHeader(simple_url_loader.get(), kHeaderEtag),
                     GetStringHeader(simple_url_loader.get(),
                                     kHeaderXCupServerProof),
                     GetInt64Header(simple_url_loader.get(),
                                    kHeaderXRetryAfter));
          },
          std::move(simple_url_loader),
          std::move(post_request_complete_callback)),
      kMaxResponseSize);
}

base::OnceClosure NetworkFetcherImpl::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  if (!cookie_predicate_.Run(url) ||
      !network::IsUrlPotentiallyTrustworthy(url)) {
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  } else {
    resource_request->site_for_cookies = net::SiteForCookies::FromUrl(url);
  }
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_url_loader->SetRetryOptions(
      kMaxRetriesOnNetworkChange,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader->SetAllowPartialResults(true);
  simple_url_loader->SetOnResponseStartedCallback(base::BindOnce(
      &NetworkFetcherImpl::OnResponseStartedCallback, base::Unretained(this),
      std::move(response_started_callback)));
  simple_url_loader->SetOnDownloadProgressCallback(base::BindRepeating(
      &NetworkFetcherImpl::OnProgressCallback, base::Unretained(this),
      std::move(progress_callback)));
  simple_url_loader->DownloadToFile(
      shared_url_network_factory_.get(),
      base::BindOnce(
          [](std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
             DownloadToFileCompleteCallback download_to_file_complete_callback,
             base::FilePath file_path) {
            std::move(download_to_file_complete_callback)
                .Run(simple_url_loader->NetError(),
                     simple_url_loader->GetContentSize());
          },
          std::move(simple_url_loader),
          std::move(download_to_file_complete_callback)),
      file_path);
  return base::DoNothing();
}

void NetworkFetcherImpl::OnResponseStartedCallback(
    ResponseStartedCallback response_started_callback,
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  std::move(response_started_callback)
      .Run(response_head.headers ? response_head.headers->response_code() : -1,
           response_head.content_length);
}

void NetworkFetcherImpl::OnProgressCallback(ProgressCallback progress_callback,
                                            uint64_t current) {
  progress_callback.Run(base::saturated_cast<int64_t>(current));
}

NetworkFetcherChromiumFactory::NetworkFetcherChromiumFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory,
    SendCookiesPredicate cookie_predicate)
    : shared_url_network_factory_(shared_url_network_factory),
      cookie_predicate_(cookie_predicate) {}

NetworkFetcherChromiumFactory::~NetworkFetcherChromiumFactory() = default;

std::unique_ptr<NetworkFetcher> NetworkFetcherChromiumFactory::Create() const {
  return std::make_unique<NetworkFetcherImpl>(shared_url_network_factory_,
                                              cookie_predicate_);
}

}  // namespace update_client
