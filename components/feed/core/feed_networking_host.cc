// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_networking_host.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/zlib/google/compression_utils.h"

using IdentityManager = identity::IdentityManager;

namespace {

constexpr char kApiKeyQueryParam[] = "key";
constexpr char kAuthenticationScope[] =
    "https://www.googleapis.com/auth/googlenow";
constexpr char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

constexpr char kContentEncoding[] = "Content-Encoding";
constexpr char kContentType[] = "application/octet-stream";
constexpr char kGzip[] = "gzip";

}  // namespace

namespace feed {

// NetworkFetch is a helper class internal to the networking host. Each
// instance represents a single "logical" fetch that ends by calling the
// associated callback. Network fetches will actually attempt two fetches if
// there is a signed in user; the first to retrieve an access token, and the
// second to the specified url.
class NetworkFetch {
 public:
  NetworkFetch(const GURL& url,
               const std::string& request_type,
               std::vector<uint8_t> request_body,
               IdentityManager* identity_manager,
               network::SharedURLLoaderFactory* loader_factory,
               const std::string& api_key);

  void Start(FeedNetworkingHost::ResponseCallback done_callback);

 private:
  void StartAccessTokenFetch();
  void AccessTokenFetchFinished(GoogleServiceAuthError error,
                                identity::AccessTokenInfo access_token_info);
  void StartLoader();
  std::unique_ptr<network::SimpleURLLoader> MakeLoader();
  net::HttpRequestHeaders MakeHeaders(const std::string& auth_header) const;
  void PopulateRequestBody(network::SimpleURLLoader* loader);
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response);

  const GURL url_;
  const std::string request_type_;
  std::string access_token_;
  const std::vector<uint8_t> request_body_;
  IdentityManager* const identity_manager_;
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  FeedNetworkingHost::ResponseCallback done_callback_;
  network::SharedURLLoaderFactory* loader_factory_;
  const std::string api_key_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetch);
};

NetworkFetch::NetworkFetch(const GURL& url,
                           const std::string& request_type,
                           std::vector<uint8_t> request_body,
                           IdentityManager* identity_manager,
                           network::SharedURLLoaderFactory* loader_factory,
                           const std::string& api_key)
    : url_(url),
      request_type_(request_type),
      request_body_(std::move(request_body)),
      identity_manager_(identity_manager),
      loader_factory_(loader_factory),
      api_key_(api_key) {}

void NetworkFetch::Start(FeedNetworkingHost::ResponseCallback done_callback) {
  done_callback_ = std::move(done_callback);

  if (!identity_manager_->HasPrimaryAccount()) {
    StartLoader();
    return;
  }

  StartAccessTokenFetch();
}

void NetworkFetch::StartAccessTokenFetch() {
  identity::ScopeSet scopes{kAuthenticationScope};
  // It's safe to pass base::Unretained(this) since deleting the token fetcher
  // will prevent the callback from being completed.
  token_fetcher_ = std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
      "feed", identity_manager_, scopes,
      base::BindOnce(&NetworkFetch::AccessTokenFetchFinished,
                     base::Unretained(this)),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void NetworkFetch::AccessTokenFetchFinished(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.Network.TokenFetchStatus",
                            error.state(), GoogleServiceAuthError::NUM_STATES);
  access_token_ = access_token_info.token;
  StartLoader();
}

void NetworkFetch::StartLoader() {
  simple_loader_ = MakeLoader();
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory_, base::BindOnce(&NetworkFetch::OnSimpleLoaderComplete,
                                      base::Unretained(this)));
}

std::unique_ptr<network::SimpleURLLoader> NetworkFetch::MakeLoader() {
  std::string auth_header =
      access_token_.empty()
          ? std::string()
          : base::StringPrintf(kAuthorizationRequestHeaderFormat,
                               access_token_.c_str());
  net::HttpRequestHeaders headers = MakeHeaders(auth_header);
  // TODO(pnoland): Add data use measurement once it's supported for simple
  // url loader.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("interest_feed_send", R"(
        semantics {
          sender: "Feed Library"
          description: "Chrome can show content suggestions (e.g. articles) "
          "in the form of a feed. For signed-in users, these may be "
          "personalized based on the user's synced browsing history."
          trigger: "Triggered periodically in the background, or upon "
          "explicit user request."
          data: "The locale of the device and data describing the suggested "
            "content that the user interacted with. For signed in users "
            "the request is authenticated. "
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This can be disabled from the New Tab Page by collapsing "
          "the articles section."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");
  GURL url(url_);
  if (access_token_.empty() && !api_key_.empty())
    url = net::AppendQueryParameter(url_, kApiKeyQueryParam, api_key_);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;

  resource_request->load_flags = net::LOAD_BYPASS_CACHE;
  resource_request->allow_credentials = false;
  resource_request->headers = headers;
  resource_request->method = request_type_;

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_loader->SetAllowHttpErrorResults(true);
  PopulateRequestBody(simple_loader.get());
  return simple_loader;
}

net::HttpRequestHeaders NetworkFetch::MakeHeaders(
    const std::string& auth_header) const {
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kContentType, kContentType);
  headers.SetHeader(kContentEncoding, kGzip);

  bool is_authorized = !auth_header.empty();
  if (is_authorized)
    headers.SetHeader(net::HttpRequestHeaders::kAuthorization, auth_header);

  variations::SignedIn signed_in_status =
      is_authorized ? variations::SignedIn::kYes : variations::SignedIn::kNo;
  // Add X-Client-Data header with experiment IDs from field trials.
  variations::AppendVariationHeaders(url_, variations::InIncognito::kNo,
                                     signed_in_status, &headers);
  return headers;
}

void NetworkFetch::PopulateRequestBody(network::SimpleURLLoader* loader) {
  std::string compressed_request_body;
  if (!request_body_.empty()) {
    std::string uncompressed_request_body(
        reinterpret_cast<const char*>(request_body_.data()),
        request_body_.size());

    compression::GzipCompress(uncompressed_request_body,
                              &compressed_request_body);

    loader->AttachStringForUpload(compressed_request_body, kContentType);
  }

  UMA_HISTOGRAM_COUNTS_1M(
      "ContentSuggestions.Feed.Network.RequestSizeKB.Compressed",
      static_cast<int>(compressed_request_body.size() / 1024));
}

void NetworkFetch::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response) {
  int32_t status_code = simple_loader_->NetError();
  std::vector<uint8_t> response_body;

  if (response) {
    status_code = simple_loader_->ResponseInfo()->headers->response_code();

    if (status_code == net::HTTP_UNAUTHORIZED) {
      identity::ScopeSet scopes{kAuthenticationScope};
      std::string account_id = identity_manager_->GetPrimaryAccountId();
      identity_manager_->RemoveAccessTokenFromCache(account_id, scopes,
                                                    access_token_);
    }

    const uint8_t* begin = reinterpret_cast<const uint8_t*>(response->data());
    const uint8_t* end = begin + response->size();
    response_body.assign(begin, end);
  }

  base::UmaHistogramSparse("ContentSuggestions.Feed.Network.RequestStatusCode",
                           status_code);

  // The below is true even if there is a protocol error, so this will
  // record response size as long as the request completed.
  if (status_code >= 200) {
    UMA_HISTOGRAM_COUNTS_1M("ContentSuggestions.Feed.Network.ResponseSizeKB",
                            static_cast<int>(response_body.size() / 1024));
  }

  std::move(done_callback_).Run(status_code, std::move(response_body));
}

FeedNetworkingHost::FeedNetworkingHost(
    identity::IdentityManager* identity_manager,
    const std::string& api_key,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : identity_manager_(identity_manager),
      api_key_(api_key),
      loader_factory_(std::move(loader_factory)) {}

FeedNetworkingHost::~FeedNetworkingHost() = default;

void FeedNetworkingHost::CancelRequests() {
  pending_requests_.clear();
}

void FeedNetworkingHost::Send(
    const GURL& url,
    const std::string& request_type,
    std::vector<uint8_t> request_body,
    ResponseCallback callback) {
  auto fetch = std::make_unique<NetworkFetch>(
      url, request_type, std::move(request_body), identity_manager_,
      loader_factory_.get(), api_key_);
  NetworkFetch* fetch_unowned = fetch.get();
  pending_requests_.emplace(std::move(fetch));

  // It's safe to pass base::Unretained(this) since deleting the network fetch
  // will prevent the callback from being completed.
  fetch_unowned->Start(base::BindOnce(&FeedNetworkingHost::NetworkFetchFinished,
                                      base::Unretained(this), fetch_unowned,
                                      std::move(callback)));
}

void FeedNetworkingHost::NetworkFetchFinished(
    NetworkFetch* fetch,
    ResponseCallback callback,
    int32_t http_code,
    std::vector<uint8_t> response_body) {
  auto fetch_iterator = pending_requests_.find(fetch);
  CHECK(fetch_iterator != pending_requests_.end());
  pending_requests_.erase(fetch_iterator);

  std::move(callback).Run(http_code, std::move(response_body));
}

}  // namespace feed
