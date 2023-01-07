// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_fetcher.h"

#include <utility>

#include "base/lazy_instance.h"
#include "components/query_tiles/internal/stats.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace query_tiles {
namespace {

// An override server URL for testing.
base::LazyInstance<GURL>::Leaky g_override_url_for_testing;

const char kRequestContentType[] = "application/x-protobuf";

constexpr net::NetworkTrafficAnnotationTag kQueryTilesFetcherTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("query_tiles_fetcher", R"(
              semantics {
                sender: "Query Tiles Fetcher"
                description:
                  "Fetches RPC for query tiles on Android NTP and omnibox."
                trigger:
                  "A priodic TileBackgroundTask will always be scheduled to "
                  "fetch RPC from server, unless the feature is disabled "
                  "or suspended."
                data: "Country code and accepted languages will be sent via "
                  "the header. No user information is sent."
                destination: GOOGLE_OWNED_SERVICE
              }
              policy {
                cookies_allowed: NO
                setting: "Disabled if a non-Google search engine is used."
                chrome_policy {
                  DefaultSearchProviderEnabled {
                    DefaultSearchProviderEnabled: false
                  }
                }
              }
    )");

class TileFetcherImpl : public TileFetcher {
 public:
  TileFetcherImpl(
      const GURL& url,
      const std::string& country_code,
      const std::string& accept_languages,
      const std::string& api_key,
      const std::string& experiment_tag,
      const std::string& client_version,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory),
        url_(url),
        country_code_(country_code),
        accept_languages_(accept_languages),
        api_key_(api_key),
        experiment_tag_(experiment_tag),
        client_version_(client_version),
        tile_info_request_status_(TileInfoRequestStatus::kInit) {}

 private:
  // TileFetcher implementation.
  void StartFetchForTiles(FinishedCallback callback) override {
    auto resource_request = BuildGetRequest();
    if (!resource_request)
      return;
    url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), kQueryTilesFetcherTrafficAnnotation);
    url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&TileFetcherImpl::OnDownloadComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Build the request to get tile info.
  std::unique_ptr<network::ResourceRequest> BuildGetRequest() {
    if (url_.is_empty() && g_override_url_for_testing.Get().is_empty())
      return nullptr;
    auto request = std::make_unique<network::ResourceRequest>();
    request->method = net::HttpRequestHeaders::kGetMethod;
    request->headers.SetHeader("x-goog-api-key", api_key_);
    request->headers.SetHeader("X-Client-Version", client_version_);
    request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                               kRequestContentType);
    if (!g_override_url_for_testing.Get().is_empty()) {
      request->url = g_override_url_for_testing.Get();
    } else {
      request->url = net::AppendOrReplaceQueryParameter(url_, "country_code",
                                                        country_code_);
      if (!experiment_tag_.empty()) {
        request->url = net::AppendOrReplaceQueryParameter(
            request->url, "experiment_tag", experiment_tag_);
      }
    }
    if (!accept_languages_.empty()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                 accept_languages_);
    }

    return request;
  }

  bool ShouldSuspendDueToNetError() {
    auto error_code = url_loader_->NetError();
    stats::RecordTileFetcherNetErrorCode(error_code);
    switch (error_code) {
      case net::ERR_BLOCKED_BY_ADMINISTRATOR:
        return true;
      default:
        return false;
    }
  }

  bool ShouldSuspend(int response_code) {
    switch (response_code) {
      case net::HTTP_NOT_IMPLEMENTED:
      case net::HTTP_FORBIDDEN:
        return true;
      default:
        return ShouldSuspendDueToNetError();
    }
  }

  // Called after receiving HTTP response. Processes the response code and net
  // error.
  void OnDownloadComplete(FinishedCallback callback,
                          std::unique_ptr<std::string> response_body) {
    int response_code = -1;
    if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
      response_code = url_loader_->ResponseInfo()->headers->response_code();

    if (response_code >= 200 && response_code < 300 && response_body) {
      tile_info_request_status_ = TileInfoRequestStatus::kSuccess;
    } else {
      tile_info_request_status_ = ShouldSuspend(response_code)
                                      ? TileInfoRequestStatus::kShouldSuspend
                                      : TileInfoRequestStatus::kFailure;
    }
    stats::RecordTileFetcherResponseCode(response_code);
    std::move(callback).Run(tile_info_request_status_,
                            std::move(response_body));
    tile_info_request_status_ = TileInfoRequestStatus::kInit;
    url_loader_.reset();
  }

  void SetServerUrl(const GURL& url) override { url_ = url; }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Simple URL loader to fetch proto from network.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Params of resource request.
  GURL url_;
  std::string country_code_;
  std::string accept_languages_;
  std::string api_key_;
  std::string experiment_tag_;
  std::string client_version_;

  // Status of the tile info request.
  TileInfoRequestStatus tile_info_request_status_;

  base::WeakPtrFactory<TileFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TileFetcher> TileFetcher::Create(
    const GURL& url,
    const std::string& country_code,
    const std::string& accept_languages,
    const std::string& api_key,
    const std::string& experiment_tag,
    const std::string& client_version,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<TileFetcherImpl>(url, country_code, accept_languages,
                                           api_key, experiment_tag,
                                           client_version, url_loader_factory);
}

// static
void TileFetcher::SetOverrideURLForTesting(const GURL& url) {
  g_override_url_for_testing.Get() = url;
}

TileFetcher::TileFetcher() = default;
TileFetcher::~TileFetcher() = default;

}  // namespace query_tiles
