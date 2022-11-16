// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_
#define COMPONENTS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

class GoogleServiceAuthError;
class GURL;

enum class FetchErrorType {
  kAuthError = 0,
  kNetError = 1,
  kResultParseError = 2,
};

struct EndpointResponse {
  std::string response;
  int http_status_code{-1};
  absl::optional<FetchErrorType> error_type;
};

using EndpointFetcherCallback =
    base::OnceCallback<void(std::unique_ptr<EndpointResponse>)>;

// EndpointFetcher calls an endpoint and returns the response.
// EndpointFetcher is not thread safe and it is up to the caller
// to wait until the callback function passed to Fetch() completes
// before invoking Fetch() again.
// Destroying an EndpointFetcher will result in the in-flight request being
// cancelled.
// EndpointFetcher performs authentication via the signed in user to
// Chrome.
// If the request times out an empty response will be returned. There will also
// be an error code indicating timeout once more detailed error messaging is
// added TODO(crbug.com/993393).
class EndpointFetcher {
 public:
  // Preferred constructor - forms identity_manager and url_loader_factory.
  // OAUTH authentication is used for this constructor.
  EndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const std::string& oauth_consumer_name,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const std::vector<std::string>& scopes,
      int64_t timeout_ms,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      signin::IdentityManager* const identity_manager);

  // Constructor if Chrome API Key is used for authentication
  EndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      int64_t timeout_ms,
      const std::string& post_data,
      const std::vector<std::string>& headers,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      bool is_stable_channel);

  // Constructor if no authentication is needed.
  EndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& annotation_tag);

  // Used for tests. Can be used if caller constructs their own
  // url_loader_factory and identity_manager.
  EndpointFetcher(
      const std::string& oauth_consumer_name,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const std::vector<std::string>& scopes,
      int64_t timeout_ms,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      signin::IdentityManager* const identity_manager);

  // This Constructor can be used in a background thread.
  EndpointFetcher(
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      int64_t timeout_ms,
      const std::string& post_data,
      const std::vector<std::string>& headers,
      const std::vector<std::string>& cors_exempt_headers,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const bool is_oauth_fetch);

  EndpointFetcher(const EndpointFetcher& endpoint_fetcher) = delete;

  EndpointFetcher& operator=(const EndpointFetcher& endpoint_fetcher) = delete;

  virtual ~EndpointFetcher();

  // TODO(crbug.com/999256) enable cancellation support
  virtual void Fetch(EndpointFetcherCallback callback);
  virtual void PerformRequest(EndpointFetcherCallback endpoint_fetcher_callback,
                              const char* key);

  std::string GetUrlForTesting();

 protected:
  // Used for Mock only. see MockEndpointFetcher class.
  explicit EndpointFetcher(
      const net::NetworkTrafficAnnotationTag& annotation_tag);

 private:
  void OnAuthTokenFetched(EndpointFetcherCallback callback,
                          GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);
  void OnResponseFetched(EndpointFetcherCallback callback,
                         std::unique_ptr<std::string> response_body);
  void OnSanitizationResult(std::unique_ptr<EndpointResponse> response,
                            EndpointFetcherCallback endpoint_fetcher_callback,
                            data_decoder::JsonSanitizer::Result result);

  enum AuthType { CHROME_API_KEY, OAUTH, NO_AUTH };
  AuthType auth_type_;

  // Members set in constructor to be passed to network::ResourceRequest or
  // network::SimpleURLLoader.
  const std::string oauth_consumer_name_;
  const GURL url_;
  const std::string http_method_;
  const std::string content_type_;
  int64_t timeout_ms_;
  const std::string post_data_;
  const std::vector<std::string> headers_;
  const std::vector<std::string> cors_exempt_headers_;
  const net::NetworkTrafficAnnotationTag annotation_tag_;
  signin::ScopeSet oauth_scopes_;

  // Members set in constructor
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  bool sanitize_response_;
  bool is_stable_channel_;

  // Members set in Fetch
  std::unique_ptr<const signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<EndpointFetcher> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_
