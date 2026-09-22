// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_manifest_fetcher.h"

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

// Limit fetched manifest size to 5MB to prevent excessive memory usage.
constexpr size_t kMaxManifestLength = 5 * 1024 * 1024;
// Only accessed on the UI thread.
size_t g_max_manifest_length = kMaxManifestLength;

}  // namespace

WebInstallManifestFetcher::WebInstallManifestFetcher(
    GURL manifest_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : manifest_url_(std::move(manifest_url)),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(manifest_url_.SchemeIs(url::kHttpsScheme) ||
        (manifest_url_.SchemeIs(url::kHttpScheme) &&
         net::IsLocalhost(manifest_url_)));
}

WebInstallManifestFetcher::~WebInstallManifestFetcher() = default;

// static
base::AutoReset<size_t>
WebInstallManifestFetcher::SetMaxManifestLengthForTesting(size_t max_length) {
  return base::AutoReset<size_t>(&g_max_manifest_length, max_length);
}

void WebInstallManifestFetcher::Fetch(FetchCallback callback) {
  CHECK(!fetch_callback_);
  fetch_callback_ = std::move(callback);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("web_install_manifest_fetcher", R"(
    semantics {
      sender: "Web Install API"
      description:
        "Fetches a web app manifest JSON file from a URL provided by a "
        "web page via the Web Install API. The manifest describes the web app "
        "to be installed."
      trigger:
        "A web page calls navigator.install() or activates an <install> "
        "element with a manifest URL parameter."
      data:
        "No user data is sent. The request fetches the JSON manifest file at "
        "the specified URL."
      destination: WEBSITE
      internal {
        contacts {
          owners: "//chrome/browser/web_applications/OWNERS"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2026-06-17"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature cannot be disabled in/by settings."
      chrome_policy {
        WebAppInstallByUserEnabled {
          WebAppInstallByUserEnabled: false
        }
      }
    })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = manifest_url_;
  resource_request->method = "GET";
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  simple_url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &WebInstallManifestFetcher::OnRedirect, weak_factory_.GetWeakPtr()));

  simple_url_loader_->SetRetryOptions(
      /*max_retries=*/3, network::SimpleURLLoader::RETRY_ON_5XX |
                             network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&WebInstallManifestFetcher::OnManifestDownloaded,
                     weak_factory_.GetWeakPtr()),
      g_max_manifest_length);
}

void WebInstallManifestFetcher::OnRedirect(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  // Manifest URLs that redirect are not supported. Abort the download
  // immediately and report a redirect error.
  simple_url_loader_.reset();
  // TODO(crbug.com/525409692): Include manifest URL telemetry.
  std::move(fetch_callback_)
      .Run(base::unexpected(WebInstallManifestFetchError::kRedirected));
}

void WebInstallManifestFetcher::OnManifestDownloaded(
    std::optional<std::string> manifest_content) {
  // TODO(crbug.com/525409692): Include manifest URL telemetry.
  simple_url_loader_.reset();

  if (!manifest_content) {
    std::move(fetch_callback_)
        .Run(base::unexpected(WebInstallManifestFetchError::kDownloadFailed));
    return;
  }

  std::move(fetch_callback_).Run(std::move(*manifest_content));
}

}  // namespace web_app
